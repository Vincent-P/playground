#include "render/base_renderer.h"

#include <exo/logger.h>

BaseRenderer BaseRenderer::create(const platform::Window &window, gfx::DeviceDescription desc)
{
    BaseRenderer renderer = {};

    auto &context = renderer.context;
    auto &physical_devices = context.physical_devices;
    auto &device = renderer.device;
    auto &surface = renderer.surface;

    // Initialize the API
    context = gfx::Context::create(true, &window);

    // Pick a GPU
    u32 i_selected = u32_invalid;
    u32 i_device = 0;
    for (auto& physical_device : physical_devices)
    {
        logger::info("Found device: {}\n", physical_device.properties.deviceName);
        if (i_device == u32_invalid && physical_device.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            logger::info("Prioritizing device {} because it is a discrete GPU.\n", physical_device.properties.deviceName);
            i_selected = i_device;
        }
        i_device += 1;
    }
    if (i_selected == u32_invalid)
    {
        i_selected = 0;
        logger::info("No discrete GPU found, defaulting to device #0: {}.\n", physical_devices[0].properties.deviceName);
    }

    desc.physical_device = &physical_devices[i_selected];

    // Create the GPU
    device = gfx::Device::create(context, desc);

    // Create an empty image to full the slot #0 on bindless descriptors
    renderer.empty_image = device.create_image({.name = "Empty image", .usages = gfx::sampled_image_usage | gfx::storage_image_usage});

    // Create the drawing surface
    surface = gfx::Surface::create(context, device, window);

    for (auto &work_pool : renderer.work_pools)
    {
        device.create_work_pool(work_pool);
    }

    for (auto &timing : renderer.timings)
    {
        timing.create(device);
    }

    // Prepare the frame synchronization
    renderer.fence = device.create_fence();

    renderer.dynamic_uniform_buffer = RingBuffer::create(device, {
            .name = "Dynamic Uniform",
            .size = 128_KiB,
            .gpu_usage = gfx::uniform_buffer_usage,
        });

    renderer.dynamic_vertex_buffer = RingBuffer::create(device, {
            .name = "Dynamic vertices",
            .size = 1_MiB,
            .gpu_usage = gfx::storage_buffer_usage,
        });

    renderer.dynamic_index_buffer = RingBuffer::create(device, {
            .name = "Dynamic indices",
            .size = 64_KiB,
            .gpu_usage = gfx::index_buffer_usage,
        });

    return renderer;
}

void BaseRenderer::destroy()
{
    device.wait_idle();

    device.destroy_fence(fence);

    for (auto &work_pool : work_pools)
    {
        device.destroy_work_pool(work_pool);
    }

    for (auto &timing : timings)
    {
        timing.destroy(device);
    }
    surface.destroy(context, device);
    device.destroy(context);
    context.destroy();
}

void* BaseRenderer::bind_shader_options(gfx::ComputeWork &cmd, Handle<gfx::GraphicsProgram> program, usize options_len)
{
    auto [options, options_offset] = dynamic_uniform_buffer.allocate(device, options_len);
    cmd.bind_uniform_buffer(program, 0, dynamic_uniform_buffer.buffer, options_offset, options_len);
    return options;
}

void* BaseRenderer::bind_shader_options(gfx::ComputeWork &cmd, Handle<gfx::ComputeProgram> program, usize options_len)
{
    auto [options, options_offset] = dynamic_uniform_buffer.allocate(device, options_len);
    cmd.bind_uniform_buffer(program, 0, dynamic_uniform_buffer.buffer, options_offset, options_len);
    return options;
}

void* BaseRenderer::bind_global_options(usize options_len)
{
    auto [options, options_offset] = dynamic_uniform_buffer.allocate(device, options_len);
    device.bind_global_uniform_buffer(dynamic_uniform_buffer.buffer, options_offset, options_len);
    return options;
}

void BaseRenderer::reload_shader(std::string_view shader_name)
{
    device.wait_idle();

    logger::info("{} changed!\n", shader_name);

    // Find the shader that needs to be updated
    gfx::Shader *found = nullptr;
    for (auto &[shader_h, shader] : device.shaders) {
        if (shader_name == shader->filename) {
            assert(found == nullptr);
            found = &(*shader);
        }
    }

    if (!found) {
        assert(false);
        return;
    }

    gfx::Shader &shader = *found;

    Vec<Handle<gfx::Shader>> to_remove;

    // Update programs using this shader to the new shader
    for (auto &[program_h, program] : device.compute_programs)
    {
        if (program->state.shader.is_valid())
        {
            auto &compute_shader = *device.shaders.get(program->state.shader);
            if (compute_shader.filename == shader.filename)
            {
                Handle<gfx::Shader> new_shader = device.create_shader(shader_name);
                logger::info("Found a program using the shader, creating the new shader module #{}\n", new_shader.value());

                to_remove.push_back(program->state.shader);
                program->state.shader = new_shader;
                device.recreate_program_internal(*program);
            }
        }
    }

    // Destroy the old shaders
    for (Handle<gfx::Shader> shader_h : to_remove) {
        logger::info("Removing old shader #{}\n", shader_h.value());
        device.destroy_shader(shader_h);
    }
    logger::info("\n");
}

void BaseRenderer::on_resize()
{
    device.wait_idle();
    surface.destroy_swapchain(device);
    surface.create_swapchain(device);
}

bool BaseRenderer::start_frame()
{
    auto current_frame = frame_count % FRAME_QUEUE_LENGTH;

    // wait for fence, blocking: dont wait for the first QUEUE_LENGTH frames
    u64 wait_value = frame_count < FRAME_QUEUE_LENGTH ? 0 : frame_count-FRAME_QUEUE_LENGTH+1;
    device.wait_for_fences({fence}, {wait_value});

    // reset the command buffers
    auto &work_pool = work_pools[current_frame];
    auto &timing = timings[current_frame];

    device.reset_work_pool(work_pool);

    timing.get_results(device);
#if 0
    for (u32 i_label = 0; i_label < timing.labels.size(); i_label += 1)
    {
        logger::info("[{}]: CPU {:.4f} ms | GPU {:.4f} ms\n", timing.labels[i_label], timing.cpu[i_label], timing.gpu[i_label]);
    }
#endif

    timing.reset(device);

    dynamic_uniform_buffer.start_frame();
    dynamic_vertex_buffer.start_frame();
    dynamic_index_buffer.start_frame();

    bool out_of_date_swapchain = device.acquire_next_swapchain(surface);
    return out_of_date_swapchain;
}

bool BaseRenderer::end_frame(gfx::ComputeWork &cmd)
{
    // vulkan hack: hint the device to submit a semaphore to wait on before presenting
    cmd.prepare_present(surface);

    device.submit(cmd, {fence}, {frame_count+1});

    // present will wait for semaphore
    bool out_of_date_swapchain = device.present(surface, cmd);
    if (out_of_date_swapchain)
    {
        return true;
    }

    frame_count += 1;
    dynamic_uniform_buffer.end_frame();
    dynamic_vertex_buffer.end_frame();
    dynamic_index_buffer.end_frame();
    return false;
}
