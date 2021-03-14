#include "render/renderer.hpp"

#include "base/logger.hpp"

#include "render/vulkan/commands.hpp"
#include "render/vulkan/device.hpp"
#include "render/vulkan/resources.hpp"
#include "render/vulkan/utils.hpp"
#include "vulkan/vulkan_core.h"

#include "ui.hpp"
#include "scene.hpp"
#include "components/mesh_component.hpp"

#include <tuple> // for std::tie
#include <imgui/imgui.h>

Renderer Renderer::create(const platform::Window &window)
{
    Renderer renderer = {};

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
    device = gfx::Device::create(context, physical_devices[i_selected]);

    // Create the drawing surface
    surface = gfx::Surface::create(context, device, window);

    for (auto &work_pool : renderer.work_pools)
    {
        device.create_work_pool(work_pool);
    }

    // Prepare the frame synchronizations
    renderer.fence = device.create_fence();

    // Create Render targets
    renderer.swapchain_rt.clear_renderpass = device.find_or_create_renderpass({.colors = {{.format = surface.format.format}}});
    renderer.swapchain_rt.load_renderpass = device.find_or_create_renderpass({.colors = {{.format = surface.format.format, .load_op = VK_ATTACHMENT_LOAD_OP_LOAD}}});

    renderer.swapchain_rt.framebuffer = device.create_framebuffer({
            .width = surface.extent.width,
            .height = surface.extent.height,
            .attachments_format = {surface.format.format},
        });

    renderer.hdr_rt.clear_renderpass = device.find_or_create_renderpass({.colors = {{.format = VK_FORMAT_R16G16B16A16_SFLOAT}}});
    renderer.hdr_rt.load_renderpass  = device.find_or_create_renderpass({.colors = {{.format = VK_FORMAT_R16G16B16A16_SFLOAT, .load_op = VK_ATTACHMENT_LOAD_OP_LOAD}}});


    Vec<gfx::DescriptorType> common_descriptors = {
        {.type = gfx::DescriptorType::DynamicBuffer, .count = 1},
    };

    // Create ImGui pass

    gfx::GraphicsState gui_state = {};
    gui_state.vertex_shader   =  device.create_shader("shaders/gui.vert.spv");
    gui_state.fragment_shader =  device.create_shader("shaders/gui.frag.spv");
    gui_state.framebuffer = renderer.swapchain_rt.framebuffer;
    gui_state.descriptors = common_descriptors;
    auto &imgui_pass = renderer.imgui_pass;
    imgui_pass.program = device.create_program(gui_state);

    gfx::RenderState state = {.alpha_blending = true};
    uint gui_default = device.compile(imgui_pass.program, state);
    UNUSED(gui_default);

    auto &io = ImGui::GetIO();
    uchar *pixels = nullptr;
    int width  = 0;
    int height = 0;
    io.Fonts->Build();
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    usize font_atlas_size = width * height * sizeof(u32);

    imgui_pass.font_atlas = device.create_image({
            .name = "Font Atlas",
            .size = {static_cast<u32>(width), static_cast<u32>(height), 1},
            .format = VK_FORMAT_R8G8B8A8_UNORM,
        });

    imgui_pass.font_atlas_staging = device.create_buffer({
            .name = "Imgui font atlas staging",
            .size = font_atlas_size,
            .usage = gfx::source_buffer_usage,
            .memory_usage = VMA_MEMORY_USAGE_CPU_ONLY,
        });

    uchar *p_font_atlas = device.map_buffer<uchar>(imgui_pass.font_atlas_staging);
    for (uint i = 0; i < font_atlas_size; i++)
    {
        p_font_atlas[i] = pixels[i];
    }
    device.flush_buffer(imgui_pass.font_atlas_staging);

    imgui_pass.vertices = device.create_buffer({
            .name = "Imgui vertices",
            .size = 1_MiB,
            .usage = gfx::storage_buffer_usage,
            .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
        });

    imgui_pass.indices = device.create_buffer({
            .name = "Imgui indices",
            .size = 1_MiB,
            .usage = gfx::index_buffer_usage,
            .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
        });

    imgui_pass.options = device.create_buffer({
            .name = "Imgui options",
            .size = sizeof(ImguiOptions),
            .usage = gfx::uniform_buffer_usage,
            .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
        });

    // Create the luminance pass
    auto &tonemap_pass = renderer.tonemap_pass;

    tonemap_pass.tonemap = device.create_program({
        .shader = device.create_shader("shaders/tonemap.comp.glsl.spv"),
        .descriptors = common_descriptors,
    });

    tonemap_pass.build_histo = device.create_program({
        .shader = device.create_shader("shaders/build_luminance_histo.comp.spv"),
        .descriptors = common_descriptors,
    });

    tonemap_pass.average_histo = device.create_program({
        .shader = device.create_shader("shaders/average_luminance_histo.comp.spv"),
        .descriptors = common_descriptors,
    });

    tonemap_pass.histogram = device.create_buffer({
        .name  = "Luminance histogram",
        .size  = 256 * sizeof(uint),
        .usage = gfx::storage_buffer_usage,
    });


    tonemap_pass.average_luminance = device.create_image({
        .name          = "Average luminance",
        .size          = {1, 1, 1},
        .type          = VK_IMAGE_TYPE_2D,
        .format        = VK_FORMAT_R32_SFLOAT,
    });


    renderer.transfer_done = device.create_fence(renderer.transfer_fence_value);

    // global set
    renderer.device.bind_global_sampled_image(0, imgui_pass.font_atlas);
    renderer.device.update_globals();

    return renderer;
}

void Renderer::destroy()
{
    device.wait_idle();

    device.destroy_fence(fence);
    device.destroy_fence(transfer_done);

    for (auto &work_pool : work_pools)
    {
        device.destroy_work_pool(work_pool);
    }

    surface.destroy(context, device);
    device.destroy(context);
    context.destroy();
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
    device.wait_for(fence, wait_value);

    // reset the command buffers
    auto &work_pool = work_pools[current_frame];
    device.reset_work_pool(work_pool);

    ImGui::Render();

    // receipt contains the image acquired semaphore
    bool out_of_date_swapchain = device.acquire_next_swapchain(surface);
    if (out_of_date_swapchain)
    {
        return true;
    }

    return false;
}

static void do_imgui_pass(gfx::Device &device, gfx::GraphicsWork& cmd, RenderTargets &output, ImGuiPass &pass_data, bool clear_rt = true)
{
    // -- Upload ImGui's vertices and indices
    ImDrawData *data = ImGui::GetDrawData();
    assert(sizeof(ImDrawVert) * static_cast<u32>(data->TotalVtxCount) < 1_MiB);
    assert(sizeof(ImDrawIdx)  * static_cast<u32>(data->TotalVtxCount) < 1_MiB);

    auto *vertices = device.map_buffer<ImDrawVert>(pass_data.vertices);
    auto *indices  = device.map_buffer<ImDrawIdx>(pass_data.indices);

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
        indices  += cmd_list.IdxBuffer.Size;
    }

    // -- Update shader data
    auto *options = device.map_buffer<ImguiOptions>(pass_data.options);
    options->scale = float2(2.0f / data->DisplaySize.x, 2.0f / data->DisplaySize.y);
    options->translation = float2(-1.0f - data->DisplayPos.x * options->scale.x, -1.0f - data->DisplayPos.y * options->scale.y);
    options->vertices_pointer = device.get_buffer_address(pass_data.vertices);
    options->texture_binding = 0; // the atlas was binded to index 0 in the global set

    cmd.barrier(pass_data.font_atlas, gfx::ImageUsage::GraphicsShaderRead);
    cmd.barrier(output.image, gfx::ImageUsage::ColorAttachment);
    cmd.begin_pass(clear_rt ? output.clear_renderpass : output.load_renderpass, output.framebuffer, {output.image}, {{{.float32 = {0.0f, 0.0f, 0.0f, 1.0f}}}});

    cmd.bind_uniform_buffer(pass_data.program, 0, pass_data.options, 0, sizeof(ImguiOptions));
    cmd.bind_pipeline(pass_data.program, 0);
    cmd.bind_index_buffer(pass_data.indices);

    float2 clip_off   = data->DisplayPos;       // (0,0) unless using multi-viewports
    float2 clip_scale = data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    VkViewport viewport{};
    viewport.width    = data->DisplaySize.x * data->FramebufferScale.x;
    viewport.height   = data->DisplaySize.y * data->FramebufferScale.y;
    viewport.minDepth = 1.0f;
    viewport.maxDepth = 1.0f;
    cmd.set_viewport(viewport);

    i32 vertex_offset = 0;
    u32 index_offset  = 0;
    for (int list = 0; list < data->CmdListsCount; list++)
    {
        const auto &cmd_list = *data->CmdLists[list];

        for (int command_index = 0; command_index < cmd_list.CmdBuffer.Size; command_index++)
        {
            const auto &draw_command = cmd_list.CmdBuffer[command_index];

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

            cmd.set_scissor(scissor);

            cmd.draw_indexed({.vertex_count = draw_command.ElemCount, .index_offset = index_offset, .vertex_offset = vertex_offset});

            index_offset += draw_command.ElemCount;
        }
        vertex_offset += cmd_list.VtxBuffer.Size;
    }

    cmd.end_pass();
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
    return false;
}


void Renderer::display_ui(UI::Context &ui)
{
    ImGuiWindowFlags fb_flags = ImGuiWindowFlags_NoDecoration;
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

        ui.end_window();
    }
}

void Renderer::update(const Scene &scene)
{
    if (settings.resolution_dirty)
    {
        device.destroy_image(hdr_rt.image);
        hdr_rt.image = device.create_image({
            .name = "luminance buffer",
            .size = {settings.render_resolution.x, settings.render_resolution.y, 1},
            .format = VK_FORMAT_R16G16B16A16_SFLOAT,
            });


        device.destroy_framebuffer(hdr_rt.framebuffer);
        hdr_rt.framebuffer = device.create_framebuffer({
                .width = settings.render_resolution.x,
                .height = settings.render_resolution.y,
                .attachments_format = {VK_FORMAT_R16G16B16A16_SFLOAT},
            });

        settings.resolution_dirty = false;
    }

    if (start_frame())
    {
        on_resize();
        return;
    }

    scene.world.for_each<MeshComponent>([&](const auto &mesh){
        const auto *model = scene.models.get(mesh.model_handle);
        if (model)
        {
            logger::info("I want to draw {} !!\n", model->path);
        }
    });

    auto current_frame = frame_count % FRAME_QUEUE_LENGTH;
    auto &work_pool = work_pools[current_frame];
    swapchain_rt.image = surface.images[surface.current_image];


    // -- Upload the font atlas during the first frame
    if (frame_count == 0)
    {
        gfx::TransferWork transfer_cmd = device.get_transfer_work(work_pool);
        transfer_cmd.begin();
        transfer_cmd.clear_barrier(imgui_pass.font_atlas, gfx::ImageUsage::TransferDst);
        transfer_cmd.copy_buffer_to_image(imgui_pass.font_atlas_staging, imgui_pass.font_atlas);
        transfer_cmd.end();
        device.submit(transfer_cmd, {transfer_done}, {transfer_fence_value+1});
    }

    gfx::GraphicsWork cmd = device.get_graphics_work(work_pool);
    cmd.begin();

    // vulkan hack: this command buffer will wait for the image acquire semaphore
    cmd.wait_for_acquired(surface, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    if (frame_count == 0)
    {
        cmd.wait_for(transfer_done, transfer_fence_value+1, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        transfer_fence_value += 1;
    }

    do_imgui_pass(device, cmd, swapchain_rt, imgui_pass);

    cmd.barrier(swapchain_rt.image, gfx::ImageUsage::Present);
    cmd.end();

    if (end_frame(cmd))
    {
        on_resize();
        return;
    }
}
