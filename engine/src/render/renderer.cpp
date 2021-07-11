#include "render/renderer.hpp"

#include "base/logger.hpp"
#include "base/intrinsics.hpp"

#include "base/numerics.hpp"
#include "camera.hpp"
#include "render/vulkan/commands.hpp"
#include "render/vulkan/device.hpp"
#include "render/vulkan/resources.hpp"
#include "render/vulkan/utils.hpp"
#include "vulkan/vulkan_core.h"
#include "render/material.hpp"

#include "asset_manager.hpp"
#include "ui.hpp"
#include "scene.hpp"
#include "components/camera_component.hpp"
#include "components/mesh_component.hpp"
#include "components/transform_component.hpp"
#include "tools.hpp"

#include <stdexcept>
#include <tuple> // for std::tie
#include <imgui/imgui.h>
#include <stb_image.h>


Renderer Renderer::create(const platform::Window &window, AssetManager *_asset_manager)
{
    Renderer renderer = {};
    renderer.asset_manager = _asset_manager;

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

    // Create the GPU
    device = gfx::Device::create(context, {
            .physical_device = &physical_devices[i_selected],
            .push_constant_layout = {.size = sizeof(PushConstants)},
            .buffer_device_address = false
        });

    // Create empty images to full the slot #0 on bindless descriptors
    renderer.empty_sampled_image = device.create_image({.name = "Empty sampled image", .usages = gfx::sampled_image_usage | gfx::storage_image_usage});
    renderer.empty_storage_image = device.create_image({.name = "Empty storage image", .usages = gfx::storage_image_usage});

    // Create the drawing surface
    surface = gfx::Surface::create(context, device, window);

    for (auto &work_pool : renderer.work_pools)
    {
        device.create_work_pool(work_pool);
    }

    for (auto &timestamp_pool : renderer.timestamp_pools)
    {
        device.create_query_pool(timestamp_pool, TIMESTAMPS_PER_FRAME);
    }

    // Prepare the frame synchronization
    renderer.fence = device.create_fence();

    renderer.dynamic_uniform_buffer = RingBuffer::create(device, {
            .name = "Dynamic Uniform",
            .size = 32_KiB,
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

    renderer.instances_data = RingBuffer::create(device, {
            .name = "Dynamic indices",
            .size = 64_MiB,
            .gpu_usage = gfx::storage_buffer_usage,
        }, false);

    // Create Render targets
    renderer.swapchain_rt.clear_renderpass = device.find_or_create_renderpass({.colors = {{.format = surface.format.format}}});
    renderer.swapchain_rt.load_renderpass  = device.find_or_create_renderpass({.colors = {{.format = surface.format.format, .load_op = VK_ATTACHMENT_LOAD_OP_LOAD}}});

    renderer.swapchain_rt.framebuffer = device.create_framebuffer({
        .width              = surface.extent.width,
        .height             = surface.extent.height,
        .attachments_format = {surface.format.format},
    });

    renderer.settings.resolution_dirty  = true;
    renderer.settings.render_resolution = {surface.extent.width, surface.extent.height};

    renderer.hdr_rt.clear_renderpass = device.find_or_create_renderpass({
        .colors = {{.format = VK_FORMAT_R32G32B32A32_SFLOAT, .load_op = VK_ATTACHMENT_LOAD_OP_CLEAR}},
        .depth  = {{.format = VK_FORMAT_D32_SFLOAT, .load_op = VK_ATTACHMENT_LOAD_OP_CLEAR}},
    });

    renderer.hdr_rt.load_renderpass = device.find_or_create_renderpass({
        .colors = {{.format = VK_FORMAT_R32G32B32A32_SFLOAT, .load_op = VK_ATTACHMENT_LOAD_OP_LOAD}},
        .depth  = {{.format = VK_FORMAT_D32_SFLOAT, .load_op = VK_ATTACHMENT_LOAD_OP_LOAD}},
    });

    renderer.ldr_rt.clear_renderpass = device.find_or_create_renderpass({
        .colors = {{.format = VK_FORMAT_R32G32B32A32_SFLOAT, .load_op = VK_ATTACHMENT_LOAD_OP_CLEAR}},
        .depth  = {{.format = VK_FORMAT_D32_SFLOAT, .load_op = VK_ATTACHMENT_LOAD_OP_LOAD}},
    });
    renderer.ldr_rt.load_renderpass  = device.find_or_create_renderpass({
        .colors = {{.format = VK_FORMAT_R32G32B32A32_SFLOAT, .load_op = VK_ATTACHMENT_LOAD_OP_LOAD}},
        .depth  = {{.format = VK_FORMAT_D32_SFLOAT, .load_op = VK_ATTACHMENT_LOAD_OP_LOAD}},
    });

    // Create ImGui pass
    auto &imgui_pass = renderer.imgui_pass;
    {
        gfx::GraphicsState gui_state = {};
        gui_state.vertex_shader   =  device.create_shader("shaders/gui.vert.spv");
        gui_state.fragment_shader =  device.create_shader("shaders/gui.frag.spv");
        gui_state.renderpass = renderer.swapchain_rt.clear_renderpass;
        gui_state.descriptors =  {
            {{.count = 1, .type = gfx::DescriptorType::DynamicBuffer}},
            {{.count = 1, .type = gfx::DescriptorType::StorageBuffer}},
        };
        imgui_pass.program = device.create_program("imgui", gui_state);

        gfx::RenderState state = {.rasterization = {.culling = false}, .alpha_blending = true};
        device.compile(imgui_pass.program, state);

        auto &io      = ImGui::GetIO();
        uchar *pixels = nullptr;
        int width     = 0;
        int height    = 0;
        io.Fonts->Build();
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        imgui_pass.font_atlas = device.create_image({
            .name   = "Font Atlas",
            .size   = {static_cast<u32>(width), static_cast<u32>(height), 1},
            .format = VK_FORMAT_R8G8B8A8_UNORM,
        });

        ImGui::GetIO().Fonts->SetTexID((void *)((u64)device.get_image_sampled_index(imgui_pass.font_atlas)));
    }

    // Create opaque program
    {
        gfx::GraphicsState state = {};
        state.vertex_shader      = device.create_shader("shaders/opaque.vert.spv");
        state.fragment_shader    = device.create_shader("shaders/opaque.frag.spv");
        state.renderpass         = renderer.hdr_rt.load_renderpass;
        state.descriptors        = {
            {.count = 1, .type = gfx::DescriptorType::DynamicBuffer}, // options
            {.count = 1, .type = gfx::DescriptorType::StorageBuffer}, // vertices
            {.count = 1, .type = gfx::DescriptorType::StorageBuffer}, // instances
        };
        renderer.opaque_program = device.create_program("gltf opaque", state);

        gfx::RenderState render_state   = {};
        render_state.depth.test         = VK_COMPARE_OP_GREATER_OR_EQUAL;
        render_state.depth.enable_write = true;
        uint opaque_default             = device.compile(renderer.opaque_program, render_state);
        UNUSED(opaque_default);
    }

    // Create tonemap program
    {
        renderer.tonemap_program = device.create_program("tonemap", {
        .shader = device.create_shader("shaders/tonemap.comp.spv"),
        .descriptors =  {
            {.count = 1, .type = gfx::DescriptorType::DynamicBuffer},
        },
    });
    }

    return renderer;
}

void Renderer::destroy()
{
    device.wait_idle();

    device.destroy_fence(fence);

    for (auto &work_pool : work_pools)
    {
        device.destroy_work_pool(work_pool);
    }

    for (auto &timestamp_pool : timestamp_pools)
    {
        device.destroy_query_pool(timestamp_pool);
    }

    streamer.destroy();
    surface.destroy(context, device);
    device.destroy(context);
    context.destroy();
}

void* Renderer::bind_shader_options(gfx::ComputeWork &cmd, Handle<gfx::GraphicsProgram> program, usize options_len)
{
    auto [options, options_offset] = dynamic_uniform_buffer.allocate(device, options_len);
    cmd.bind_uniform_buffer(program, 0, dynamic_uniform_buffer.buffer, options_offset, options_len);
    return options;
}

void* Renderer::bind_shader_options(gfx::ComputeWork &cmd, Handle<gfx::ComputeProgram> program, usize options_len)
{
    auto [options, options_offset] = dynamic_uniform_buffer.allocate(device, options_len);
    cmd.bind_uniform_buffer(program, 0, dynamic_uniform_buffer.buffer, options_offset, options_len);
    return options;
}

void Renderer::reload_shader(std::string_view shader_name)
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

void Renderer::on_resize()
{
    device.wait_idle();
    surface.destroy_swapchain(device);
    surface.create_swapchain(device);

    device.destroy_framebuffer(swapchain_rt.framebuffer);
    swapchain_rt.framebuffer = device.create_framebuffer({
            .width = surface.extent.width,
            .height = surface.extent.height,
            .attachments_format = {surface.format.format},
        });
}

bool Renderer::start_frame()
{
    auto current_frame = frame_count % FRAME_QUEUE_LENGTH;

    // wait for fence, blocking: dont wait for the first QUEUE_LENGTH frames
    u64 wait_value = frame_count < FRAME_QUEUE_LENGTH ? 0 : frame_count-FRAME_QUEUE_LENGTH+1;
    streamer.wait();
    device.wait_for_fences({fence}, {wait_value});

    // reset the command buffers
    auto &work_pool = work_pools[current_frame];
    auto &timestamp_pool = timestamp_pools[current_frame];

    device.reset_work_pool(work_pool);

    timestamps.clear();
    device.get_query_results(timestamp_pool, 0, 2, timestamps);

    logger::info("frame time: {} ms\n", 1.e-6f * device.get_ns_per_timestamp() * (timestamps[1] - timestamps[0]));

    device.reset_query_pool(timestamp_pool, 0, TIMESTAMPS_PER_FRAME);

    dynamic_uniform_buffer.start_frame();
    dynamic_vertex_buffer.start_frame();
    dynamic_index_buffer.start_frame();
    instances_data.start_frame();

    bool out_of_date_swapchain = device.acquire_next_swapchain(surface);
    return out_of_date_swapchain;
}

bool Renderer::end_frame(gfx::ComputeWork &cmd)
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
    instances_data.end_frame();
    return false;
}

static void recreate_framebuffers(Renderer &r)
{
    auto &device   = r.device;
    auto &settings = r.settings;

    device.wait_idle();

    device.destroy_image(r.depth_buffer);
    r.depth_buffer = device.create_image({
        .name   = "Depth buffer",
        .size   = {settings.render_resolution.x, settings.render_resolution.y, 1},
        .format = VK_FORMAT_D32_SFLOAT,
        .usages = gfx::depth_attachment_usage,
    });

    device.destroy_image(r.hdr_rt.image);
    r.hdr_rt.image = device.create_image({
        .name   = "HDR buffer",
        .size   = {settings.render_resolution.x, settings.render_resolution.y, 1},
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .usages = gfx::color_attachment_usage,
    });
    r.hdr_rt.depth = r.depth_buffer;

    device.destroy_framebuffer(r.hdr_rt.framebuffer);
    r.hdr_rt.framebuffer = device.create_framebuffer({
        .width              = settings.render_resolution.x,
        .height             = settings.render_resolution.y,
        .attachments_format = {VK_FORMAT_R32G32B32A32_SFLOAT},
        .depth_format       = VK_FORMAT_D32_SFLOAT,
    });

    device.destroy_image(r.ldr_rt.image);
    r.ldr_rt.image = device.create_image({
        .name   = "LDR buffer",
        .size   = {settings.render_resolution.x, settings.render_resolution.y, 1},
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usages = gfx::color_attachment_usage,
    });

    device.destroy_framebuffer(r.ldr_rt.framebuffer);
    r.ldr_rt.framebuffer = device.create_framebuffer({
        .width              = settings.render_resolution.x,
        .height             = settings.render_resolution.y,
        .attachments_format = {VK_FORMAT_R8G8B8A8_UNORM},
    });
}

void Renderer::display_ui(UI::Context &ui)
{
    ImGuiWindowFlags fb_flags = 0;// ImGuiWindowFlags_NoDecoration;
    if (ui.begin_window("Framebuffer", true, fb_flags))
    {
        float2 max = ImGui::GetWindowContentRegionMax();
        float2 min = ImGui::GetWindowContentRegionMin();
        float2 size = float2(min.x < max.x ? max.x - min.x : min.x, min.y < max.y ? max.y - min.y : min.y);

        if (static_cast<uint>(size.x) != settings.render_resolution.x || static_cast<uint>(size.y) != settings.render_resolution.y)
        {
            settings.render_resolution.x = static_cast<uint>(size.x);
            settings.render_resolution.y = static_cast<uint>(size.y);
            settings.resolution_dirty = true;
        }

        ImGui::Image((void*)((u64)device.get_image_sampled_index(ldr_rt.image)), size);

        ui.end_window();
    }

    if (ui.begin_window("Textures"))
    {
        for (uint i = 5; i <= 8; i += 1)
        {
            ImGui::Text("[%u]", i);
            ImGui::Image((void*)((u64)i), float2(256.0f, 256.0f));
        }


        ui.end_window();
    }

    if (ui.begin_window("Shaders"))
    {
        ui.end_window();
    }

    if (ui.begin_window("Settings"))
    {
        if (ImGui::CollapsingHeader("Renderer"))
        {
            ImGui::Checkbox("Enable TAA", &settings.enable_taa);
            ImGui::Checkbox("Enable Path tracing", &settings.enable_path_tracing);
        }
        ui.end_window();
    }
}

void Renderer::update(Scene &scene)
{
    // -- Handle resize
    if (start_frame())
    {
        on_resize();
        ImGui::EndFrame();
        return;
    }

    if (settings.resolution_dirty)
    {
        recreate_framebuffers(*this);
        settings.resolution_dirty = false;
    }

    auto current_frame = frame_count % FRAME_QUEUE_LENGTH;
    auto &work_pool    = work_pools[current_frame];
    auto &timestamp_pool = timestamp_pools[current_frame];
    swapchain_rt.image = surface.images[surface.current_image];


    // -- Transfer stuff
    if (frame_count == 0)
    {
        auto &io      = ImGui::GetIO();
        uchar *pixels = nullptr;
        int width     = 0;
        int height    = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        streamer.init(&device);
        streamer.upload(imgui_pass.font_atlas, pixels, width * height * sizeof(u32));
    }
    streamer.update(work_pool);

    // -- Get the main camera
    CameraComponent *main_camera              = nullptr;
    TransformComponent *main_camera_transform = nullptr;
    scene.world.for_each<TransformComponent, CameraComponent>(
        [&](auto &transform, auto &camera)
        {
            if (!main_camera)
            {
                main_camera           = &camera;
                main_camera_transform = &transform;
            }
        });
    assert(main_camera != nullptr);
    main_camera->projection = camera::infinite_perspective(main_camera->fov, (float)settings.render_resolution.x / settings.render_resolution.y, main_camera->near_plane, &main_camera->projection_inverse);

    // -- Get geometry from the scene and prepare the draw commands
    render_instances.clear();

    scene.world.for_each<LocalToWorldComponent, RenderMeshComponent>(
        [&](LocalToWorldComponent &local_to_world_component, RenderMeshComponent &render_mesh_component)
        {
            if (render_mesh_component.i_mesh >= this->render_meshes.size())
            {
                for (u32 i_mesh = this->render_meshes.size(); i_mesh < asset_manager->meshes.size(); i_mesh += 1)
                {
                    auto &mesh_asset = asset_manager->meshes[i_mesh];

                    logger::info("Uploading mesh asset #{}\n", i_mesh);

                    RenderMesh render_mesh   = {};
                    render_mesh.positions    = device.create_buffer({
                        .name  = "Positions buffer",
                        .size  = mesh_asset.positions.size() * sizeof(float4),
                        .usage = gfx::storage_buffer_usage,
                    });
                    render_mesh.indices      = device.create_buffer({
                        .name  = "Index buffer",
                        .size  = mesh_asset.indices.size() * sizeof(u32),
                        .usage = gfx::index_buffer_usage,
                    });
                    render_mesh.vertex_count = static_cast<u32>(mesh_asset.indices.size());
                    render_mesh.submeshes    = mesh_asset.submeshes;

                    streamer.upload(render_mesh.positions, mesh_asset.positions.data(), mesh_asset.positions.size() * sizeof(float4));
                    streamer.upload(render_mesh.indices, mesh_asset.indices.data(), mesh_asset.indices.size() * sizeof(u32));

                    this->render_meshes.push_back(render_mesh);
                }
            }
            else
            {
                render_instances.push_back({
                    .transform     = local_to_world_component.transform,
                    .i_render_mesh = render_mesh_component.i_mesh,
                });
            }
        });

    Vec<u32> instances_to_draw;
    for (u32 i_render_instance = 0; i_render_instance < render_instances.size(); i_render_instance += 1)
    {
        const auto &render_instance = render_instances[i_render_instance];
        const auto &render_mesh     = render_meshes[render_instance.i_render_mesh];
        if (!streamer.is_uploaded(render_mesh.positions) || !streamer.is_uploaded(render_mesh.indices))
        {
            continue;
        }
        instances_to_draw.push_back(i_render_instance);
    }

    auto [p_instances, instance_offset] = instances_data.allocate(device, instances_to_draw.size() * sizeof(RenderInstance));
    auto *p_instances_data              = reinterpret_cast<RenderInstance *>(p_instances);
    for (u32 i_to_draw = 0; i_to_draw < instances_to_draw.size(); i_to_draw += 1)
    {
        u32 i_render_instance       = instances_to_draw[i_to_draw];
        p_instances_data[i_to_draw] = render_instances[i_render_instance];
    }

    // -- Update global data
    static float4x4 last_view = main_camera->view;
    static float4x4 last_proj = main_camera->projection;

    auto [global_data, global_offset]       = dynamic_uniform_buffer.allocate<GlobalUniform>(device);
    global_data->camera_view                = main_camera->view;
    global_data->camera_projection          = main_camera->projection;
    global_data->camera_view_inverse        = main_camera->view_inverse;
    global_data->camera_projection_inverse  = main_camera->projection_inverse;
    global_data->camera_previous_view       = last_view;
    global_data->camera_previous_projection = last_proj;
    global_data->resolution                 = float2(float(settings.render_resolution.x), float(settings.render_resolution.y));
    global_data->delta_t                    = 0.016f;
    global_data->frame_count                = frame_count;
    global_data->jitter_offset              = float2(0.0);

    last_view = main_camera->view;
    last_proj = main_camera->projection;

    device.bind_global_uniform_buffer(dynamic_uniform_buffer.buffer, global_offset, sizeof(GlobalUniform));
    device.update_globals();

    // -- Do the actual rendering
    gfx::GraphicsWork cmd = device.get_graphics_work(work_pool);
    cmd.begin();
    cmd.bind_global_set();

    cmd.timestamp_query(timestamp_pool, 0);

    // vulkan only: this command buffer will wait for the image acquire semaphore
    cmd.wait_for_acquired(surface, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkViewport viewport{};
    viewport.width    = float(settings.render_resolution.x);
    viewport.height   = float(settings.render_resolution.y);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    cmd.set_viewport(viewport);

    VkRect2D scissor = {};
    scissor.extent.width  = settings.render_resolution.x;
    scissor.extent.height = settings.render_resolution.y;
    cmd.set_scissor(scissor);

    // Opaque pass
    cmd.barrier(depth_buffer, gfx::ImageUsage::DepthAttachment);
    cmd.barrier(hdr_rt.image, gfx::ImageUsage::ColorAttachment);
    cmd.begin_pass(hdr_rt.clear_renderpass, hdr_rt.framebuffer, {hdr_rt.image, depth_buffer}, {{{.float32 = {0.0f, 0.0f, 0.0f, 1.0f}}}, {{.float32 = {0.0f, 0.0f, 0.0f, 0.0f}}}});

    struct PACKED OpaqueOptions
    {
        u32 first_instance;
    };

    auto *options           = this->bind_shader_options<OpaqueOptions>(cmd, opaque_program);
    options->first_instance = instance_offset / static_cast<u32>(sizeof(RenderInstance));

    uint i_draw = 0;
    for (auto i_render_instance : instances_to_draw)
    {
        const auto &render_instance = render_instances[i_render_instance];
        const auto &render_mesh     = render_meshes[render_instance.i_render_mesh];

        cmd.bind_index_buffer(render_mesh.indices, VK_INDEX_TYPE_UINT32);
        cmd.bind_storage_buffer(opaque_program, 1, render_mesh.positions);
        cmd.bind_storage_buffer(opaque_program, 2, instances_data.buffer);
        cmd.bind_pipeline(opaque_program, 0);

        for (const auto &submesh : render_mesh.submeshes)
        {
            cmd.push_constant<PushConstants>({.draw_id = i_draw});
            cmd.draw_indexed({.vertex_count = submesh.index_count, .index_offset = submesh.first_index, .instance_offset = i_render_instance});
            i_draw += 1;
        }
    }

    cmd.end_pass();

    // Tonemap
    {
        cmd.barrier(hdr_rt.image, gfx::ImageUsage::ComputeShaderRead);
        cmd.clear_barrier(ldr_rt.image, gfx::ImageUsage::ComputeShaderReadWrite);

        auto hdr_buffer_size = device.get_image_size(hdr_rt.image);

        auto *options                 = this->bind_shader_options<TonemapOptions>(cmd, tonemap_program);
        options->sampled_hdr_buffer   = device.get_image_sampled_index(hdr_rt.image);
        options->storage_output_frame = device.get_image_storage_index(ldr_rt.image);
        cmd.bind_pipeline(tonemap_program);
        cmd.dispatch({hdr_buffer_size.x / 16, hdr_buffer_size.y / 16, 1});
    }

    // ImGui pass
    ImGui::Render();
    if (streamer.is_uploaded(imgui_pass.font_atlas))
    {
        ImDrawData *data = ImGui::GetDrawData();

        // -- Prepare Imgui draw commands
        assert(sizeof(ImDrawVert) * static_cast<u32>(data->TotalVtxCount) < 1_MiB);
        assert(sizeof(ImDrawIdx) * static_cast<u32>(data->TotalVtxCount) < 1_MiB);

        u32 vertices_size = data->TotalVtxCount * sizeof(ImDrawVert);
        u32 indices_size  = data->TotalIdxCount * sizeof(ImDrawIdx);

        auto [p_vertices, vert_offset] = dynamic_vertex_buffer.allocate(device, vertices_size);
        auto *vertices                 = reinterpret_cast<ImDrawVert *>(p_vertices);

        auto [p_indices, ind_offset] = dynamic_index_buffer.allocate(device, indices_size);
        auto *indices                = reinterpret_cast<ImDrawIdx *>(p_indices);

        struct ImguiDrawCommand
        {
            u32 texture_id;
            u32 vertex_count;
            u32 index_offset;
            i32 vertex_offset;
            VkRect2D scissor;
        };

        Vec<ImguiDrawCommand> draws;
        float2 clip_off   = data->DisplayPos;       // (0,0) unless using multi-viewports
        float2 clip_scale = data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

        u32 i_draw        = 0;
        i32 vertex_offset = 0;
        u32 index_offset  = 0;
        for (int i = 0; i < data->CmdListsCount; i++)
        {
            const auto &cmd_list = *data->CmdLists[i];

            for (int i_vertex = 0; i_vertex < cmd_list.VtxBuffer.Size; i_vertex++)
            {
                vertices[i_vertex] = cmd_list.VtxBuffer.Data[i_vertex];
            }

            for (int i_index = 0; i_index < cmd_list.IdxBuffer.Size; i_index++)
            {
                indices[i_index] = cmd_list.IdxBuffer.Data[i_index];
            }

            vertices += cmd_list.VtxBuffer.Size;
            indices += cmd_list.IdxBuffer.Size;

            // Check that texture are correct
            for (int command_index = 0; command_index < cmd_list.CmdBuffer.Size; command_index++)
            {
                const auto &draw_command = cmd_list.CmdBuffer[command_index];

                // Prepare the texture to be sampled
                u32 texture_id = static_cast<u32>((u64)draw_command.TextureId);

                // Project scissor/clipping rectangles into framebuffer space
                ImVec4 clip_rect;
                clip_rect.x = (draw_command.ClipRect.x - clip_off.x) * clip_scale.x;
                clip_rect.y = (draw_command.ClipRect.y - clip_off.y) * clip_scale.y;
                clip_rect.z = (draw_command.ClipRect.z - clip_off.x) * clip_scale.x;
                clip_rect.w = (draw_command.ClipRect.w - clip_off.y) * clip_scale.y;

                // Apply scissor/clipping rectangle
                VkRect2D scissor;
                scissor.offset.x      = (static_cast<i32>(clip_rect.x) > 0) ? static_cast<i32>(clip_rect.x) : 0;
                scissor.offset.y      = (static_cast<i32>(clip_rect.y) > 0) ? static_cast<i32>(clip_rect.y) : 0;
                scissor.extent.width  = static_cast<u32>(clip_rect.z - clip_rect.x);
                scissor.extent.height = static_cast<u32>(clip_rect.w - clip_rect.y);

                draws.push_back({.texture_id = texture_id, .vertex_count = draw_command.ElemCount, .index_offset = index_offset, .vertex_offset = vertex_offset, .scissor = scissor});
                i_draw += 1;

                index_offset += draw_command.ElemCount;
            }
            vertex_offset += cmd_list.VtxBuffer.Size;
        }

        // -- Rendering
        struct PACKED ImguiOptions
        {
            float2 scale;
            float2 translation;
            u64 vertices_pointer;
            u32 first_vertex;
            u32 pad1;
        };

        auto *options = bind_shader_options<ImguiOptions>(cmd, imgui_pass.program);
        std::memset(options, 0, sizeof(ImguiOptions));
        options->scale            = float2(2.0f / data->DisplaySize.x, 2.0f / data->DisplaySize.y);
        options->translation      = float2(-1.0f - data->DisplayPos.x * options->scale.x, -1.0f - data->DisplayPos.y * options->scale.y);
        options->first_vertex     = vert_offset / static_cast<u32>(sizeof(ImDrawVert));
        options->vertices_pointer = 0;

        // Barriers
        for (i_draw = 0; i_draw < draws.size(); i_draw += 1)
        {
            const auto &draw = draws[i_draw];
            cmd.barrier(device.get_global_sampled_image(draw.texture_id), gfx::ImageUsage::GraphicsShaderRead);
        }

        // Draw pass
        cmd.barrier(swapchain_rt.image, gfx::ImageUsage::ColorAttachment);
        cmd.begin_pass(swapchain_rt.clear_renderpass, swapchain_rt.framebuffer, {swapchain_rt.image}, {{{.float32 = {0.0f, 0.0f, 0.0f, 1.0f}}}});

        VkViewport viewport{};
        viewport.width    = data->DisplaySize.x * data->FramebufferScale.x;
        viewport.height   = data->DisplaySize.y * data->FramebufferScale.y;
        viewport.minDepth = 1.0f;
        viewport.maxDepth = 1.0f;
        cmd.set_viewport(viewport);

        cmd.bind_storage_buffer(imgui_pass.program, 1, dynamic_vertex_buffer.buffer);
        cmd.bind_pipeline(imgui_pass.program, 0);
        cmd.bind_index_buffer(dynamic_index_buffer.buffer, VK_INDEX_TYPE_UINT16, ind_offset);

        for (i_draw = 0; i_draw < draws.size(); i_draw += 1)
        {
            const auto &draw = draws[i_draw];
            cmd.set_scissor(draw.scissor);
            cmd.push_constant<PushConstants>({.draw_id = i_draw, .gui_texture_id = draw.texture_id});
            cmd.draw_indexed({.vertex_count = draw.vertex_count, .index_offset = draw.index_offset, .vertex_offset = draw.vertex_offset});
        }

        cmd.end_pass();
    }

    cmd.barrier(swapchain_rt.image, gfx::ImageUsage::Present);

    cmd.timestamp_query(timestamp_pool, 1);
    cmd.end();

    if (end_frame(cmd))
    {
        on_resize();
        return;
    }
}
