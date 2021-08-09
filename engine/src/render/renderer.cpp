#include "render/renderer.h"

#include "asset_manager.h"
#include "camera.h"
#include "render/bvh.h"
#include "render/mesh.h"
#include "render/unified_buffer_storage.h"
#include "render/vulkan/resources.h"
#include "ui.h"
#include "scene.h"
#include "components/camera_component.h"
#include "components/transform_component.h"
#include "components/mesh_component.h"

#include <exo/logger.h>
#include <exo/quaternion.h>
#include <vulkan/vulkan_core.h>

static uint3 dispatch_size(uint3 size, u32 threads)
{
    return {
        (size.x / threads) + uint(size.x % threads != 0),
        (size.y / threads) + uint(size.y % threads != 0),
        (size.z / threads) + uint(size.z % threads != 0),
    };
}

Renderer Renderer::create(const platform::Window &window, AssetManager *_asset_manager)
{
    Renderer renderer = {};
    renderer.asset_manager = _asset_manager;
    renderer.base_renderer = BaseRenderer::create(window, {
            .push_constant_layout = {.size = sizeof(PushConstants)},
            .buffer_device_address = false,
        });

    auto &device  = renderer.base_renderer.device;
    auto &surface = renderer.base_renderer.surface;

    renderer.instances_data = RingBuffer::create(device, {
            .name = "Instances data",
            .size = 64_MiB,
            .gpu_usage = gfx::storage_buffer_usage,
        }, false);

    renderer.submesh_instances_data = RingBuffer::create(device, {
            .name = "Submesh Instances data",
            .size = 8_MiB,
            .gpu_usage = gfx::storage_buffer_usage,
        }, false);

    renderer.render_meshes_buffer = device.create_buffer({
            .name = "Meshes description buffer",
            .size = 2_MiB,
            .usage = gfx::storage_buffer_usage,
            .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
        });

    renderer.tlas_buffer = device.create_buffer({
        .name         = "TLAS BVH buffer",
        .size         = 32_MiB,
        .usage        = gfx::storage_buffer_usage,
        .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    });

    renderer.draw_arguments = device.create_buffer({
            .name = "Indirect Draw arguments",
            .size = 2_MiB,
            .usage = gfx::storage_buffer_usage | gfx::indirext_buffer_usage,
        });

    renderer.index_buffer = UnifiedBufferStorage::create(device, "Unified index buffer", 256_MiB, sizeof(u32), gfx::index_buffer_usage);

    // Create Render targets
    renderer.settings.resolution_dirty  = true;
    renderer.settings.render_resolution = {surface.extent.width, surface.extent.height};

    // Create ImGui pass
    auto &imgui_pass = renderer.imgui_pass;
    {
        gfx::GraphicsState gui_state = {};
        gui_state.vertex_shader   =  device.create_shader("shaders/gui.vert.spv");
        gui_state.fragment_shader =  device.create_shader("shaders/gui.frag.spv");
        gui_state.attachments_format = {.attachments_format = {VK_FORMAT_R8G8B8A8_UNORM}};
        gui_state.descriptors =  {
            {{.count = 1, .type = gfx::DescriptorType::DynamicBuffer}},
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
        state.attachments_format = {.attachments_format = {VK_FORMAT_R16G16B16A16_SFLOAT}, .depth_format = VK_FORMAT_D32_SFLOAT};
        state.descriptors        = {
            {.count = 1, .type = gfx::DescriptorType::DynamicBuffer}, // options
        };
        renderer.opaque_program = device.create_program("gltf opaque", state);

        gfx::RenderState render_state   = {};
        render_state.depth.test         = VK_COMPARE_OP_GREATER_OR_EQUAL;
        render_state.depth.enable_write = true;
        render_state.rasterization.culling = false;
        device.compile(renderer.opaque_program, render_state);
    }

    // Create tonemap program
    renderer.taa_program = device.create_program("taa", {
        .shader = device.create_shader("shaders/taa.comp.spv"),
        .descriptors =  {
            {.count = 1, .type = gfx::DescriptorType::DynamicBuffer},
        },
    });

    renderer.tonemap_program = device.create_program("tonemap", {
        .shader = device.create_shader("shaders/tonemap.comp.spv"),
        .descriptors =  {
            {.count = 1, .type = gfx::DescriptorType::DynamicBuffer},
        },
    });

    renderer.path_tracer_program = device.create_program("path tracer", {
        .shader = device.create_shader("shaders/path_tracer.comp.spv"),
        .descriptors =  {
            {.count = 1, .type = gfx::DescriptorType::DynamicBuffer},
        },
    });

    renderer.gen_draw_calls_program = device.create_program("gen draw calls", {
        .shader = device.create_shader("shaders/gen_draw_calls.comp.spv"),
        .descriptors =  {
            {.count = 1, .type = gfx::DescriptorType::DynamicBuffer},
        },
    });


    auto compute_halton = [](int index, int radix)
        {
            float result = 0.f;
            float fraction = 1.f / float(radix);

            while (index > 0)
            {
                result += float(index % radix) * fraction;

                index /= radix;
                fraction /= float(radix);
            }

            return result;
        };

    for (usize i_halton = 0; i_halton < ARRAY_SIZE(renderer.halton_sequence); i_halton++)
    {
        renderer.halton_sequence[i_halton].x = compute_halton(i_halton + 1, 2);
        renderer.halton_sequence[i_halton].y = compute_halton(i_halton + 1, 3);
    }

    return renderer;
}

void Renderer::destroy()
{
    streamer.destroy();
    base_renderer.destroy();
}

static void recreate_framebuffers(Renderer &r)
{
    auto &device   = r.base_renderer.device;
    auto &surface  = r.base_renderer.surface;
    auto &settings = r.settings;

    device.wait_idle();

    settings.render_resolution = {surface.extent.width, surface.extent.height};

    uint3 scaled_resolution = {};
    scaled_resolution.x = settings.resolution_scale * settings.render_resolution.x;
    scaled_resolution.y = settings.resolution_scale * settings.render_resolution.y;
    scaled_resolution.z = 1;

    // Re-create images
    device.destroy_image(r.depth_buffer);
    device.destroy_image(r.hdr_buffer);
    device.destroy_image(r.ldr_buffer);
    device.destroy_image(r.history_buffers[0]);
    device.destroy_image(r.history_buffers[1]);

    r.depth_buffer = device.create_image({
        .name   = "Depth buffer",
        .size   = scaled_resolution,
        .format = VK_FORMAT_D32_SFLOAT,
        .usages = gfx::depth_attachment_usage,
    });

    r.hdr_buffer = device.create_image({
        .name   = "HDR buffer",
        .size   = scaled_resolution,
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .usages = gfx::color_attachment_usage,
    });

    r.ldr_buffer = device.create_image({
        .name   = "LDR buffer",
        .size   = {settings.render_resolution.x, settings.render_resolution.y, 1},
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usages = gfx::color_attachment_usage,
    });

    for (u32 i_history = 0; i_history < 2; i_history += 1)
    {
        r.history_buffers[i_history] = device.create_image({
            .name   = fmt::format("History buffer #{}", i_history),
            .size   = {settings.render_resolution.x, settings.render_resolution.y, 1},
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .usages = gfx::storage_image_usage,
        });
    }

    // Re-create framebuffers
    device.destroy_framebuffer(r.hdr_depth_fb);
    device.destroy_framebuffer(r.ldr_depth_fb);
    device.destroy_framebuffer(r.ldr_fb);

    r.hdr_depth_fb = device.create_framebuffer(
        {
            .width  = scaled_resolution.x,
            .height = scaled_resolution.y,
        },
        {r.hdr_buffer},
        r.depth_buffer);

    r.ldr_depth_fb = device.create_framebuffer(
        {
            .width  = scaled_resolution.x,
            .height = scaled_resolution.y,
        },
        {r.ldr_buffer},
        r.depth_buffer);

    r.ldr_fb = device.create_framebuffer(
        {
            .width  = settings.render_resolution.x,
            .height = settings.render_resolution.y,
        },
        {r.ldr_buffer});
}

void Renderer::on_resize()
{
    base_renderer.on_resize();
    recreate_framebuffers(*this);
}

void Renderer::reload_shader(std::string_view shader_name)
{
    base_renderer.reload_shader(shader_name);
}

bool Renderer::start_frame()
{
    streamer.wait();
    bool out_of_date_swapchain = base_renderer.start_frame();
    instances_data.start_frame();
    submesh_instances_data.start_frame();
    return out_of_date_swapchain;
}

bool Renderer::end_frame(gfx::ComputeWork &cmd)
{
    bool out_of_date_swapchain = base_renderer.end_frame(cmd);
    if (out_of_date_swapchain)
    {
        return true;
    }

    instances_data.end_frame();
    submesh_instances_data.end_frame();
    return false;
}

void Renderer::display_ui(UI::Context &ui)
{
    auto &device = base_renderer.device;

    ImGuiWindowFlags fb_flags = 0;// ImGuiWindowFlags_NoDecoration;

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
            if (ImGui::SliderFloat("Resolution scale", &settings.resolution_scale, 0.25f, 1.0f))
            {
                settings.resolution_dirty = true;
            }
            ImGui::Checkbox("Enable TAA", &settings.enable_taa);
            if (ImGui::Checkbox("TAA: Clear history", &settings.clear_history))
            {
                first_accumulation_frame = base_renderer.frame_count;
            }
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

    auto &device         = base_renderer.device;
    auto current_frame   = base_renderer.frame_count % FRAME_QUEUE_LENGTH;
    auto &work_pool      = base_renderer.work_pools[current_frame];
    auto &timings        = base_renderer.timings[current_frame];
    auto swapchain_image = base_renderer.surface.images[base_renderer.surface.current_image];

    // -- Transfer stuff
    if (base_renderer.frame_count == 0)
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
    this->prepare_geometry(scene);

    // -- Update global data
    float2 current_sample = halton_sequence[base_renderer.frame_count%16] - float2(0.5);

    static float4x4 last_view = main_camera->view;
    static float4x4 last_proj = main_camera->projection;

    auto *global_data                              = base_renderer.bind_global_options<GlobalUniform>();
    global_data->camera_view                       = main_camera->view;
    global_data->camera_projection                 = main_camera->projection;
    global_data->camera_view_inverse               = main_camera->view_inverse;
    global_data->camera_projection_inverse         = main_camera->projection_inverse;
    global_data->camera_previous_view              = last_view;
    global_data->camera_previous_projection        = last_proj;
    global_data->render_resolution                 = float2(std::floor(settings.resolution_scale * settings.render_resolution.x), std::floor(settings.resolution_scale * settings.render_resolution.y));
    global_data->jitter_offset                     = current_sample;
    global_data->delta_t                           = 0.016f;
    global_data->frame_count                       = base_renderer.frame_count;
    global_data->first_accumulation_frame          = this->first_accumulation_frame;
    global_data->meshes_data_descriptor            = device.get_buffer_storage_index(render_meshes_buffer);
    global_data->instances_data_descriptor         = device.get_buffer_storage_index(instances_data.buffer);
    global_data->instances_offset                  = this->instances_offset;
    global_data->submesh_instances_data_descriptor = device.get_buffer_storage_index(submesh_instances_data.buffer);
    global_data->submesh_instances_offset          = this->submesh_instances_offset;
    global_data->tlas_descriptor                   = device.get_buffer_storage_index(tlas_buffer);
    global_data->submesh_instances_count           = this->submesh_instances_to_draw.size();
    global_data->index_buffer_descriptor           = device.get_buffer_storage_index(this->index_buffer.buffer);

    last_view = main_camera->view;
    last_proj = main_camera->projection;

    device.update_globals();

    // -- Do the actual rendering
    gfx::GraphicsWork cmd = device.get_graphics_work(work_pool);
    cmd.begin();
    cmd.bind_global_set();
    cmd.bind_index_buffer(this->index_buffer.buffer, VK_INDEX_TYPE_UINT32);

    timings.begin_label(cmd, "Frame");

    // vulkan only: this command buffer will wait for the image acquire semaphore
    cmd.wait_for_acquired(base_renderer.surface, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkViewport viewport{};
    viewport.width    = std::floor(settings.resolution_scale * float(settings.render_resolution.x));
    viewport.height   = std::floor(settings.resolution_scale * float(settings.render_resolution.y));
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    cmd.set_viewport(viewport);

    VkRect2D scissor      = {};
    scissor.extent.width  = settings.resolution_scale * settings.render_resolution.x;
    scissor.extent.height = settings.resolution_scale * settings.render_resolution.y;
    cmd.set_scissor(scissor);

    // Opaque pass
    if (settings.enable_path_tracing)
    {
        cmd.barrier(hdr_buffer, gfx::ImageUsage::ComputeShaderReadWrite);

        struct PACKED PathTracerOptions
        {
            u32 storage_output;
        };

        auto *options           = base_renderer.bind_shader_options<PathTracerOptions>(cmd, path_tracer_program);
        options->storage_output = device.get_image_storage_index(hdr_buffer);

        auto hdr_buffer_size = device.get_image_size(hdr_buffer);

        cmd.bind_pipeline(path_tracer_program);
        cmd.dispatch(dispatch_size(hdr_buffer_size, 16));
    }
    else
    {
        {
            struct PACKED GenDrawCallOptions
            {
                u32 draw_arguments_descriptor;
            };

            auto *options                      = base_renderer.bind_shader_options<GenDrawCallOptions>(cmd, gen_draw_calls_program);
            options->draw_arguments_descriptor = device.get_buffer_storage_index(draw_arguments);

            cmd.bind_pipeline(gen_draw_calls_program);
            cmd.dispatch(dispatch_size({this->draw_count, 1, 1}, 32));
        }

        cmd.clear_barrier(hdr_buffer, gfx::ImageUsage::ColorAttachment);
        cmd.clear_barrier(depth_buffer, gfx::ImageUsage::DepthAttachment);
        cmd.begin_pass(hdr_depth_fb, {gfx::LoadOp::clear({.color = {.float32 = {0.0f, 0.0f, 0.0f, 0.0f}}}), gfx::LoadOp::clear({.depthStencil = {.depth = 0.0f}})});

        {
            struct PACKED OpaqueOptions
            {
                u32 nothing;
            };
            auto *options                      = base_renderer.bind_shader_options<OpaqueOptions>(cmd, opaque_program);
            cmd.bind_pipeline(opaque_program, 0);
            cmd.draw_indexed_indirect_count({.arguments_buffer = draw_arguments, .arguments_offset = sizeof(u32), .count_buffer = draw_arguments, .max_draw_count = this->draw_count});
        }

        cmd.end_pass();
    }

    auto &current_history = history_buffers[current_frame%2];
    auto &previous_history = history_buffers[(current_frame+1)%2];

    if (settings.clear_history)
    {
        cmd.clear_barrier(previous_history, gfx::ImageUsage::TransferDst);
        cmd.clear_image(previous_history, {.float32 = {0.0f, 0.0f, 0.0f, 0.0f}});
        this->first_accumulation_frame = base_renderer.frame_count;
    }

    // TAA
    {
        cmd.barrier(hdr_buffer, gfx::ImageUsage::ComputeShaderRead);
        cmd.barrier(previous_history, gfx::ImageUsage::ComputeShaderRead);
        cmd.clear_barrier(current_history, gfx::ImageUsage::ComputeShaderReadWrite);

        auto history_size = device.get_image_size(current_history);

        struct PACKED TAAOptions
        {
            uint sampled_hdr_buffer;
            uint sampled_previous_history;
            uint storage_current_history;
        };

        auto *options                     = base_renderer.bind_shader_options<TAAOptions>(cmd, taa_program);
        options->sampled_hdr_buffer       = device.get_image_sampled_index(hdr_buffer);
        options->sampled_previous_history = device.get_image_sampled_index(previous_history);
        options->storage_current_history  = device.get_image_storage_index(current_history);
        cmd.bind_pipeline(taa_program);
        cmd.dispatch(dispatch_size(history_size, 16));
    }

    // Tonemap
    auto tonemap_input = current_history;
    {

        cmd.barrier(tonemap_input, gfx::ImageUsage::ComputeShaderRead);
        cmd.clear_barrier(ldr_buffer, gfx::ImageUsage::ComputeShaderReadWrite);

        auto input_size = device.get_image_size(tonemap_input);

        struct PACKED TonemapOptions
        {
            uint sampled_input;
            uint storage_output_frame;
        };

        auto *options                 = base_renderer.bind_shader_options<TonemapOptions>(cmd, tonemap_program);
        options->sampled_input        = device.get_image_sampled_index(tonemap_input);
        options->storage_output_frame = device.get_image_storage_index(ldr_buffer);
        cmd.bind_pipeline(tonemap_program);
        cmd.dispatch(dispatch_size(input_size, 16));
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

        auto [p_vertices, vert_offset] = base_renderer.dynamic_vertex_buffer.allocate(device, vertices_size);
        auto *vertices                 = reinterpret_cast<ImDrawVert *>(p_vertices);

        auto [p_indices, ind_offset] = base_renderer.dynamic_index_buffer.allocate(device, indices_size);
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
            u32 vertices_descriptor_index;
        };

        auto *options = base_renderer.bind_shader_options<ImguiOptions>(cmd, imgui_pass.program);
        std::memset(options, 0, sizeof(ImguiOptions));
        options->scale            = float2(2.0f / data->DisplaySize.x, 2.0f / data->DisplaySize.y);
        options->translation      = float2(-1.0f - data->DisplayPos.x * options->scale.x, -1.0f - data->DisplayPos.y * options->scale.y);
        options->vertices_pointer = 0;
        options->first_vertex     = vert_offset / static_cast<u32>(sizeof(ImDrawVert));
        options->vertices_descriptor_index = device.get_buffer_storage_index(base_renderer.dynamic_vertex_buffer.buffer);

        // Barriers
        for (i_draw = 0; i_draw < draws.size(); i_draw += 1)
        {
            const auto &draw = draws[i_draw];
            cmd.barrier(device.get_global_sampled_image(draw.texture_id), gfx::ImageUsage::GraphicsShaderRead);
        }

        // Draw pass
        cmd.barrier(ldr_buffer, gfx::ImageUsage::ColorAttachment);
        cmd.barrier(depth_buffer, gfx::ImageUsage::DepthAttachment);
        cmd.begin_pass(ldr_fb, {gfx::LoadOp::load()});

        VkViewport viewport{};
        viewport.width    = data->DisplaySize.x * data->FramebufferScale.x;
        viewport.height   = data->DisplaySize.y * data->FramebufferScale.y;
        viewport.minDepth = 1.0f;
        viewport.maxDepth = 1.0f;
        cmd.set_viewport(viewport);

        cmd.bind_pipeline(imgui_pass.program, 0);
        cmd.bind_index_buffer(base_renderer.dynamic_index_buffer.buffer, VK_INDEX_TYPE_UINT16, ind_offset);

        for (i_draw = 0; i_draw < draws.size(); i_draw += 1)
        {
            const auto &draw = draws[i_draw];
            cmd.set_scissor(draw.scissor);
            cmd.push_constant<PushConstants>({.draw_id = i_draw, .gui_texture_id = draw.texture_id});
            cmd.draw_indexed({.vertex_count = draw.vertex_count, .index_offset = draw.index_offset, .vertex_offset = draw.vertex_offset});
        }

        cmd.end_pass();
    }

    cmd.barrier(ldr_buffer, gfx::ImageUsage::TransferSrc);
    cmd.clear_barrier(swapchain_image, gfx::ImageUsage::TransferDst);
    cmd.blit_image(ldr_buffer, swapchain_image);
    cmd.barrier(swapchain_image, gfx::ImageUsage::Present);

    timings.end_label(cmd);
    cmd.end();

    if (end_frame(cmd))
    {
        on_resize();
        return;
    }
}

void Renderer::prepare_geometry(Scene &scene)
{
    auto &device = base_renderer.device;

    render_instances.clear();
    for (auto &render_mesh : render_meshes)
    {
        render_mesh.instances.clear();
    }


    // Upload new models and collect mesh instances from the scene
    constexpr u32 UPLOAD_PER_FRAME = 4;
    u32 i_upload = 0;
    scene.world.for_each<LocalToWorldComponent, RenderMeshComponent>(
        [&](LocalToWorldComponent &local_to_world_component, RenderMeshComponent &render_mesh_component)
        {
            if (render_mesh_component.i_mesh < this->render_meshes.size())
            {
                render_meshes[render_mesh_component.i_mesh].instances.push_back(render_instances.size());

                float4x4 translation = float4x4::identity();
                translation.at(0, 3) = local_to_world_component.translation.x;
                translation.at(1, 3) = local_to_world_component.translation.y;
                translation.at(2, 3) = local_to_world_component.translation.z;

                float4x4 rotation = float4x4_from_quaternion(local_to_world_component.quaternion);

                float4x4 scale = float4x4::identity();
                scale.at(0, 0) = local_to_world_component.scale.x;
                scale.at(1, 1) = local_to_world_component.scale.y;
                scale.at(2, 2) = local_to_world_component.scale.z;

                float4x4 otw = translation * rotation * scale;

                translation.at(0, 3) = - local_to_world_component.translation.x;
                translation.at(1, 3) = - local_to_world_component.translation.y;
                translation.at(2, 3) = - local_to_world_component.translation.z;

                scale.at(0, 0) = 1.0f / local_to_world_component.scale.x;
                scale.at(1, 1) = 1.0f / local_to_world_component.scale.y;
                scale.at(2, 2) = 1.0f / local_to_world_component.scale.z;

                rotation = transpose(rotation);

                float4x4 wto = scale * rotation * translation;

                render_instances.push_back({
                    .object_to_world     = otw,
                    .world_to_object     = wto,
                    .i_render_mesh       = render_mesh_component.i_mesh,
                });
            }
            else if (i_upload < UPLOAD_PER_FRAME)
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
                    render_mesh.submeshes      = device.create_buffer({
                        .name  = "Submeshes buffer",
                        .size  = mesh_asset.submeshes.size() * sizeof(SubMesh),
                        .usage = gfx::storage_buffer_usage,
                    });

                    BVH bvh         = create_blas(mesh_asset.indices, mesh_asset.positions);
                    render_mesh.bvh = device.create_buffer({
                        .name  = "BLAS BVH",
                        .size  = bvh.nodes.size() * sizeof(BVHNode),
                        .usage = gfx::storage_buffer_usage,
                    });
                    render_mesh.bvh_root = bvh.nodes[0];

                    RenderMeshGPU gpu        = {};
                    gpu.positions_descriptor = device.get_buffer_storage_index(render_mesh.positions);
                    gpu.first_index          = this->index_buffer.allocate(mesh_asset.indices.size());
                    gpu.bvh_descriptor       = device.get_buffer_storage_index(render_mesh.bvh);
                    gpu.submeshes_descriptor = device.get_buffer_storage_index(render_mesh.submeshes);

                    streamer.upload(render_mesh.positions, mesh_asset.positions.data(), mesh_asset.positions.size() * sizeof(float4));
                    streamer.upload(this->index_buffer.buffer, mesh_asset.indices.data(), mesh_asset.indices.size() * sizeof(u32), gpu.first_index * sizeof(u32));
                    streamer.upload(render_mesh.bvh, bvh.nodes.data(), bvh.nodes.size() * sizeof(BVHNode));
                    streamer.upload(render_mesh.submeshes, mesh_asset.submeshes.data(), mesh_asset.submeshes.size() * sizeof(SubMesh));

                    auto *meshes_gpu = reinterpret_cast<RenderMeshGPU*>(device.map_buffer(this->render_meshes_buffer));
                    assert(this->render_meshes.size() < 2_MiB / sizeof(RenderMeshGPU));
                    meshes_gpu[this->render_meshes.size()] = gpu;

                    this->render_meshes.push_back(render_mesh);

                    i_upload += 1;
                }
            }
        });

    // Gather all submesh instances and instances data from the meshes and instances lists
    submesh_instances_to_draw.clear();
    instances_to_draw.clear();
    this->draw_count = 0;
    for (u32 i_render_mesh = 0; i_render_mesh < render_meshes.size(); i_render_mesh += 1)
    {
        auto &render_mesh = render_meshes[i_render_mesh];
        auto &mesh_asset  = asset_manager->meshes[i_render_mesh];
        if (!streamer.is_uploaded(render_mesh.positions) || render_mesh.instances.empty())
        {
            continue;
        }

        render_mesh.first_instance = instances_to_draw.size();
        for (u32 i_instance : render_mesh.instances)
        {
            for (u32 i_submesh = 0; i_submesh < mesh_asset.submeshes.size(); i_submesh += 1)
            {
                SubMeshInstance submesh_instance = {};
                submesh_instance.i_mesh          = i_render_mesh;
                submesh_instance.i_submesh       = i_submesh;
                submesh_instance.i_instance      = submesh_instances_to_draw.size();
                submesh_instance.i_draw          = this->draw_count + i_submesh;
                submesh_instances_to_draw.push_back(submesh_instance);
            }
            instances_to_draw.push_back(i_instance);
        }
        this->draw_count += mesh_asset.submeshes.size();
    }

    // Upload all data to draw
    auto [p_instances, instance_offset] = instances_data.allocate(device, instances_to_draw.size() * sizeof(RenderInstance));
    auto *p_instances_data              = reinterpret_cast<RenderInstance *>(p_instances);
    for (u32 i_to_draw = 0; i_to_draw < instances_to_draw.size(); i_to_draw += 1)
    {
        u32 i_render_instance                 = instances_to_draw[i_to_draw];
        p_instances_data[i_to_draw]           = render_instances[i_render_instance];
    }
    this->instances_offset = instance_offset / static_cast<u32>(sizeof(RenderInstance));

    usize submesh_instances_size = submesh_instances_to_draw.size() * sizeof(SubMesh);
    auto [p_submesh_instances, submesh_instance_offset] = submesh_instances_data.allocate(device, submesh_instances_size);
    std::memcpy(p_submesh_instances, submesh_instances_to_draw.data(), submesh_instances_size);
    this->submesh_instances_offset = submesh_instance_offset / static_cast<u32>(sizeof(SubMeshInstance));

    // Build and upload the TLAS
    Vec<BVHNode> roots;
    Vec<float4x4> transforms;
    Vec<u32> indices;
    roots.resize(instances_to_draw.size());
    transforms.resize(instances_to_draw.size());
    indices.resize(instances_to_draw.size());
    for (u32 i_draw = 0; i_draw < instances_to_draw.size(); i_draw += 1)
    {
        u32 i_instance              = instances_to_draw[i_draw];
        const auto &render_instance = render_instances[i_instance];
        const auto &render_mesh     = render_meshes[render_instance.i_render_mesh];

        roots[i_draw]      = render_mesh.bvh_root;
        transforms[i_draw] = render_instance.object_to_world;
        indices[i_draw]    = i_draw;
    }
    BVH tlas = create_tlas(roots, transforms, indices);
    auto *tlas_gpu = reinterpret_cast<BVHNode *>(device.map_buffer(tlas_buffer));
    assert(tlas.nodes.size() * sizeof(BVHNode) < 32_MiB);
    std::memcpy(tlas_gpu, tlas.nodes.data(), tlas.nodes.size() * sizeof(BVHNode));
}
