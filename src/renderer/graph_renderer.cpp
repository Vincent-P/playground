#include "app.hpp"
#include "camera.hpp"
#include "renderer/hl_api.hpp"
#include "renderer/renderer.hpp"
#include "timer.hpp"
#include <vulkan/vulkan_core.h>

namespace my_app
{

Renderer::CheckerBoardFloorPass create_floor_pass(vulkan::API &api);
Renderer::ImGuiPass create_imgui_pass(vulkan::API &api);

Renderer Renderer::create(const Window &window, Camera &camera, TimerData &timer, UI::Context &ui)
{
    Renderer r;
    r.api   = vulkan::API::create(window);
    r.graph = RenderGraph::create(r.api);

    auto &api = r.api;

    r.p_ui     = &ui;
    r.p_window = &window;
    r.p_camera = &camera;
    r.p_timer  = &timer;


    r.imgui              = create_imgui_pass(api);
    r.checkerboard_floor = create_floor_pass(api);

    r.depth_buffer = r.graph.image_descs.add({.name = "Depth Buffer", .format = VK_FORMAT_D32_SFLOAT});

    return r;
}

void Renderer::destroy()
{
    api.wait_idle();
    api.destroy();
}

void Renderer::on_resize(int width, int height)
{
    api.on_resize(width, height);
}

void Renderer::wait_idle()
{
    api.wait_idle();
}

void Renderer::reload_shader(std::string_view)
{
}

/// --- Checker board floor

Renderer::CheckerBoardFloorPass create_floor_pass(vulkan::API &api)
{
    Renderer::CheckerBoardFloorPass pass;

    /// --- Create the index and vertex buffer

    std::array<u16, 6> indices = {0, 1, 2, 0, 2, 3};

    // clang-format off
    float height = -0.001f;
    std::array vertices =
    {
        -1.0f,  height, -1.0f,      0.0f, 0.0f,
        1.0f,  height, -1.0f,      1.0f, 0.0f,
        1.0f,  height,  1.0f,      1.0f, 1.0f,
        -1.0f,  height,  1.0f,      0.0f, 1.0f,
    };
    // clang-format on

    pass.index_buffer = api.create_buffer({
        .name  = "Floor Index buffer",
        .size  = indices.size() * sizeof(u16),
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    });

    pass.vertex_buffer = api.create_buffer({
        .name  = "Floor Vertex buffer",
        .size  = vertices.size() * sizeof(float),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    });

    api.upload_buffer(pass.index_buffer, indices.data(), indices.size() * sizeof(u16));
    api.upload_buffer(pass.vertex_buffer, vertices.data(), vertices.size() * sizeof(float));

    /// --- Create program

    vulkan::GraphicsProgramInfo pinfo{};
    pinfo.vertex_shader   = api.create_shader("shaders/checkerboard_floor.vert.spv");
    pinfo.fragment_shader = api.create_shader("shaders/checkerboard_floor.frag.spv");

    // voxel options
    pinfo.binding({.set    = vulkan::GLOBAL_DESCRIPTOR_SET,
                   .slot   = 0,
                   .stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
                   .type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC});

    pinfo.vertex_stride(3 * sizeof(float) + 2 * sizeof(float));
    pinfo.vertex_info({.format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0});
    pinfo.vertex_info({.format = VK_FORMAT_R32G32_SFLOAT, .offset = 3 * sizeof(float)});

    pinfo.enable_depth_write = true;
    pinfo.depth_test         = VK_COMPARE_OP_GREATER_OR_EQUAL;
    pinfo.depth_bias         = 0.0f;

    pass.program = api.create_program(std::move(pinfo));

    return pass;
}

void add_floor_pass(Renderer &r)
{
    auto &graph = r.graph;

    graph.add_pass({
        .name = "Checkerboard Floor pass",
        .type = PassType::Graphics,
        .color_attachment = graph.swapchain, // clear
        .depth_attachment = r.depth_buffer, // clear
        .exec = [=](RenderGraph& /*graph*/, RenderPass &/*self*/, vulkan::API &api)
        {
            auto swapchain_extent = api.ctx.swapchain.extent;
            auto program = r.checkerboard_floor.program;

            api.set_viewport_and_scissor(swapchain_extent.width, swapchain_extent.height);

            api.bind_buffer(program, vulkan::GLOBAL_DESCRIPTOR_SET, 0, r.global_uniform_pos);
            api.bind_program(program);
            api.bind_index_buffer(r.checkerboard_floor.index_buffer);
            api.bind_vertex_buffer(r.checkerboard_floor.vertex_buffer);

            api.draw_indexed(6, 1, 0, 0, 0);
        }
    });
}

/// --- ImGui Pass


Renderer::ImGuiPass create_imgui_pass(vulkan::API &api)
{
    Renderer::ImGuiPass pass;

    // Init context
    ImGui::CreateContext();

    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigDockingWithShift = false;
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
    io.BackendPlatformName = "custom_glfw";

    // Add fonts
    io.Fonts->AddFontDefault();
    ImFontConfig config;
    config.MergeMode                   = true;
    config.GlyphMinAdvanceX            = 13.0f; // Use if you want to make the icon monospaced
    static const ImWchar icon_ranges[] = {eva_icons::MIN, eva_icons::MAX, 0};
    io.Fonts->AddFontFromFileTTF("../fonts/Eva-Icons.ttf", 13.0f, &config, icon_ranges);

    // Create vulkan programs
    vulkan::GraphicsProgramInfo pinfo{};
    pinfo.vertex_shader   = api.create_shader("shaders/gui.vert.spv");
    pinfo.fragment_shader = api.create_shader("shaders/gui.frag.spv");

    pinfo.push_constant({.stages = VK_SHADER_STAGE_VERTEX_BIT, .size = 4 * sizeof(float)});

    pinfo.binding({.set    = vulkan::SHADER_DESCRIPTOR_SET,
                   .slot   = 0,
                   .stages = VK_SHADER_STAGE_FRAGMENT_BIT,
                   .type   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER});

    pinfo.vertex_stride(sizeof(ImDrawVert));

    pinfo.vertex_info({.format = VK_FORMAT_R32G32_SFLOAT, .offset = MEMBER_OFFSET(ImDrawVert, pos)});
    pinfo.vertex_info({.format = VK_FORMAT_R32G32_SFLOAT, .offset = MEMBER_OFFSET(ImDrawVert, uv)});
    pinfo.vertex_info({.format = VK_FORMAT_R8G8B8A8_UNORM, .offset = MEMBER_OFFSET(ImDrawVert, col)});

    vulkan::GraphicsProgramInfo puintinfo = pinfo;
    puintinfo.fragment_shader             = api.create_shader("shaders/gui_uint.frag.spv");

    pass.float_program = api.create_program(std::move(pinfo));
    pass.uint_program  = api.create_program(std::move(puintinfo));

    // Upload the font atlas to the GPU
    uchar *pixels = nullptr;

    int w = 0;
    int h = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);

    pass.font_atlas = api.create_image({
        .name   = "ImGui font atlas",
        .width  = static_cast<uint>(w),
        .height = static_cast<uint>(h),
    });

    api.upload_image(pass.font_atlas, pixels, w * h * 4);

    // TODO clean this somehow
    // Transition the image from the TRANSFER layout to the SHADER READ layout, upload_image doesnt do it automatically
    auto &vkimage = api.get_image(pass.font_atlas);
    auto src      = vulkan::get_src_image_access(vkimage.usage);
    auto dst      = vulkan::get_src_image_access(vulkan::ImageUsage::GraphicsShaderRead);

    VkImageMemoryBarrier b = vulkan::get_image_barrier(vkimage.vkhandle, src, dst, vkimage.full_range);
    vkimage.usage          = vulkan::ImageUsage::GraphicsShaderRead;

    auto cmd_buffer = api.get_temp_cmd_buffer();
    cmd_buffer.begin();
    vkCmdPipelineBarrier(cmd_buffer.vkhandle, src.stage, dst.stage, 0, 0, nullptr, 0, nullptr, 1, &b);
    cmd_buffer.submit_and_wait();

    return pass;
}

void add_imgui_pass(Renderer &r)
{
    ImGui::Render();
    ImDrawData *data = ImGui::GetDrawData();
    if (data == nullptr || data->TotalVtxCount == 0) {
        return;
    }

    auto &graph = r.graph;

    // The render graph needs to know about external images to put barriers on them correctly
    // are external images always going to be sampled or they need to be in differents categories
    // like regular images from the graph?
    std::vector<vulkan::ImageH> external_images;

    for (int list = 0; list < data->CmdListsCount; list++) {
        const auto &cmd_list = *data->CmdLists[list];

        for (int command_index = 0; command_index < cmd_list.CmdBuffer.Size; command_index++) {
            const auto &draw_command = cmd_list.CmdBuffer[command_index];

            if (draw_command.TextureId) {
                auto image_h = vulkan::ImageH(static_cast<u32>(reinterpret_cast<u64>(draw_command.TextureId)));
                external_images.push_back(image_h);
            }
        }
    }

    graph.add_pass({
        .name = "ImGui pass",
        .type = PassType::Graphics,
        .external_images = external_images,
        .color_attachment = graph.swapchain,
        .exec = [pass_data = r.imgui](RenderGraph& /*graph*/, RenderPass &/*self*/, vulkan::API &api)
        {
            ImDrawData *data = ImGui::GetDrawData();

            /// --- Prepare index and vertex buffer
            auto v_pos = api.dynamic_vertex_buffer(sizeof(ImDrawVert) * static_cast<u32>(data->TotalVtxCount));
            auto i_pos = api.dynamic_index_buffer(sizeof(ImDrawIdx) * static_cast<u32>(data->TotalIdxCount));

            auto *vertices = reinterpret_cast<ImDrawVert *>(v_pos.mapped);
            auto *indices  = reinterpret_cast<ImDrawIdx *>(i_pos.mapped);

            for (int i = 0; i < data->CmdListsCount; i++) {
                const auto &cmd_list = *data->CmdLists[i];

                std::memcpy(vertices, cmd_list.VtxBuffer.Data, sizeof(ImDrawVert) * size_t(cmd_list.VtxBuffer.Size));
                std::memcpy(indices, cmd_list.IdxBuffer.Data, sizeof(ImDrawIdx) * size_t(cmd_list.IdxBuffer.Size));

                vertices += cmd_list.VtxBuffer.Size;
                indices += cmd_list.IdxBuffer.Size;
            }

            float4 scale_and_translation;
            scale_and_translation[0] = 2.0f / data->DisplaySize.x;                            // X Scale
            scale_and_translation[1] = 2.0f / data->DisplaySize.y;                            // Y Scale
            scale_and_translation[2] = -1.0f - data->DisplayPos.x * scale_and_translation[0]; // X Translation
            scale_and_translation[3] = -1.0f - data->DisplayPos.y * scale_and_translation[1]; // Y Translation

            // Will project scissor/clipping rectangles into framebuffer space
            ImVec2 clip_off   = data->DisplayPos;       // (0,0) unless using multi-viewports
            ImVec2 clip_scale = data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

            VkViewport viewport{};
            viewport.width    = data->DisplaySize.x * data->FramebufferScale.x;
            viewport.height   = data->DisplaySize.y * data->FramebufferScale.y;
            viewport.minDepth = 1.0f;
            viewport.maxDepth = 1.0f;
            api.set_viewport(viewport);

            api.bind_vertex_buffer(v_pos);
            api.bind_index_buffer(i_pos);

            /// --- Draws

            enum UIProgram {
                Float,
                Uint
            };

            // Render GUI
            i32 vertex_offset = 0;
            u32 index_offset  = 0;
            for (int list = 0; list < data->CmdListsCount; list++) {
                const auto &cmd_list = *data->CmdLists[list];

                for (int command_index = 0; command_index < cmd_list.CmdBuffer.Size; command_index++) {
                    const auto &draw_command = &cmd_list.CmdBuffer[command_index];

                    vulkan::GraphicsProgramH current = pass_data.float_program;

                    if (draw_command->TextureId) {
                        auto texture = vulkan::ImageH(static_cast<u32>(reinterpret_cast<u64>(draw_command->TextureId)));
                        auto& image = api.get_image(texture);

                        if (image.info.format == VK_FORMAT_R32_UINT)
                        {
                            current = pass_data.uint_program;
                        }

                        api.bind_image(current, vulkan::SHADER_DESCRIPTOR_SET, 0, texture);
                    }
                    else {
                        api.bind_image(current, vulkan::SHADER_DESCRIPTOR_SET, 0, pass_data.font_atlas);
                    }

                    api.bind_program(current);
                    api.push_constant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float4), &scale_and_translation);

                    // Project scissor/clipping rectangles into framebuffer space
                    ImVec4 clip_rect;
                    clip_rect.x = (draw_command->ClipRect.x - clip_off.x) * clip_scale.x;
                    clip_rect.y = (draw_command->ClipRect.y - clip_off.y) * clip_scale.y;
                    clip_rect.z = (draw_command->ClipRect.z - clip_off.x) * clip_scale.x;
                    clip_rect.w = (draw_command->ClipRect.w - clip_off.y) * clip_scale.y;

                    // Apply scissor/clipping rectangle
                    // FIXME: We could clamp width/height based on clamped min/max values.
                    VkRect2D scissor;
                    scissor.offset.x      = (static_cast<i32>(clip_rect.x) > 0) ? static_cast<i32>(clip_rect.x) : 0;
                    scissor.offset.y      = (static_cast<i32>(clip_rect.y) > 0) ? static_cast<i32>(clip_rect.y) : 0;
                    scissor.extent.width  = static_cast<u32>(clip_rect.z - clip_rect.x);
                    scissor.extent.height = static_cast<u32>(clip_rect.w - clip_rect.y + 1); // FIXME: Why +1 here?

                    api.set_scissor(scissor);

                    api.draw_indexed(draw_command->ElemCount, 1, index_offset, vertex_offset, 0);

                    index_offset += draw_command->ElemCount;
                }
                vertex_offset += cmd_list.VtxBuffer.Size;
            }
        }
    });
}

/// --- BIG MESS

// it a mess
void imgui_update(Renderer &r)
{

    ImGuiIO &io        = ImGui::GetIO();
    auto &api    = r.api;
    auto &window = *r.p_window;
    auto &ui     = *r.p_ui;
    auto &timer  = *r.p_timer;

    io.DeltaTime = timer.get_delta_time();
    io.Framerate = timer.get_average_fps();

    io.DisplaySize.x             = float(api.ctx.swapchain.extent.width);
    io.DisplaySize.y             = float(api.ctx.swapchain.extent.height);
    io.DisplayFramebufferScale.x = window.get_dpi_scale().x;
    io.DisplayFramebufferScale.y = window.get_dpi_scale().y;

    if (ui.begin_window("Profiler", true))
    {
        #if 0
        static bool show_fps = false;

        if (ImGui::RadioButton("FPS", show_fps))
        {
            show_fps = true;
        }

        ImGui::SameLine();

        if (ImGui::RadioButton("ms", !show_fps))
        {
            show_fps = false;
        }

        if (show_fps)
        {
            ImGui::SetCursorPosX(20.0f);

            const auto &histogram = timer.get_fps_histogram();
            ImGui::PlotHistogram("",
                                 histogram.data(),
                                 static_cast<int>(histogram.size()),
                                 0,
                                 nullptr,
                                 0.0f,
                                 FLT_MAX,
                                 ImVec2(85.0f, 30.0f));
        }
        else
        {
            ImGui::SetCursorPosX(20.0f);

            const auto &histogram = timer.get_delta_time_histogram();
            ImGui::PlotHistogram("",
                                 histogram.data(),
                                 static_cast<int>(histogram.size()),
                                 0,
                                 nullptr,
                                 0.0f,
                                 FLT_MAX,
                                 ImVec2(85.0f, 30.0f));
        }
        #endif
        ImGui::Text("%7.1f fps", double(timer.get_average_fps()));
        ImGui::Text("%9.3f ms", double(timer.get_average_delta_time()));

        const auto &timestamps = api.timestamps;
        if (timestamps.size() > 0)
        {
            ImGui::Columns(3, "timestamps"); // 4-ways, with border
            ImGui::Separator();
            ImGui::Text("Label");
            ImGui::NextColumn();
            ImGui::Text("GPU (us)");
            ImGui::NextColumn();
            ImGui::Text("CPU (ms)");
            ImGui::NextColumn();
            ImGui::Separator();
            for (uint32_t i = 1; i < timestamps.size() - 1; i++)
            {
                auto gpu_delta = timestamps[i].gpu_microseconds - timestamps[i - 1].gpu_microseconds;
                auto cpu_delta = timestamps[i].cpu_milliseconds - timestamps[i - 1].cpu_milliseconds;

                ImGui::Text("%*s", static_cast<int>(timestamps[i].label.size()), timestamps[i].label.data());
                ImGui::NextColumn();
                ImGui::Text("%.1f", gpu_delta);
                ImGui::NextColumn();
                ImGui::Text("%.1f", cpu_delta);
                ImGui::NextColumn();
            }

            ImGui::Columns(1);
            ImGui::Separator();

            // scrolling data and average computing
            static float gpu_values[128];
            static float cpu_values[128];

            gpu_values[127]   = float(timestamps.back().gpu_microseconds - timestamps.front().gpu_microseconds);
            cpu_values[127]   = float(timestamps.back().cpu_milliseconds - timestamps.front().cpu_milliseconds);
            float gpu_average = gpu_values[0];
            float cpu_average = cpu_values[0];
            for (uint i = 0; i < 128 - 1; i++)
            {
                gpu_values[i] = gpu_values[i + 1];
                gpu_average += gpu_values[i];
                cpu_values[i] = cpu_values[i + 1];
                cpu_average += cpu_values[i];
            }
            gpu_average /= 128;
            cpu_average /= 128;

            ImGui::Text("%-17s: %7.1f us", "Total GPU time", gpu_average);
            ImGui::PlotLines("", gpu_values, 128, 0, "", 0.0f, 30000.0f, ImVec2(0, 80));

            ImGui::Text("%-17s: %7.1f ms", "Total CPU time", cpu_average);
            ImGui::PlotLines("", cpu_values, 128, 0, "", 0.0f, 30000.0f, ImVec2(0, 80));
        }

        ui.end_window();
    }
}

// it a mess
void update_uniforms(Renderer &r)
{
    auto &api = r.api;
    api.begin_label("Update uniforms");

    float aspect_ratio = api.ctx.swapchain.extent.width / float(api.ctx.swapchain.extent.height);
    static float fov   = 60.0f;
    static float s_near  = 1.0f;
    static float s_far   = 200.0f;

    r.p_camera->perspective(fov, aspect_ratio, s_near, 200.f);
    // r.p_camera->update_view();

    r.sun.position = float3(0.0f, 40.0f, 0.0f);
    r.sun.ortho_square(40.f, 1.f, 100.f);

    r.global_uniform_pos     = api.dynamic_uniform_buffer(sizeof(GlobalUniform));
    auto *globals            = reinterpret_cast<GlobalUniform *>(r.global_uniform_pos.mapped);
    std::memset(globals, 0, sizeof(GlobalUniform));

    globals->camera_pos      = r.p_camera->position;
    globals->camera_view     = r.p_camera->get_view();
    globals->camera_proj     = r.p_camera->get_projection();
    globals->camera_inv_proj = glm::inverse(globals->camera_proj);
    globals->camera_inv_view_proj = glm::inverse(globals->camera_proj * globals->camera_view);
    globals->sun_view        = r.sun.get_view();
    globals->sun_proj        = r.sun.get_projection();

    globals->resolution = uint2(api.ctx.swapchain.extent.width, api.ctx.swapchain.extent.height);
    globals->sun_direction = float4(-r.sun.front, 1);


    static float s_sun_illuminance = 10000.0f;
    static float s_multiple_scattering = 0.0f;
    if (r.p_ui->begin_window("Globals"))
    {
        ImGui::SliderFloat("Near plane", &s_near, 0.01f, 1.f);
        ImGui::SliderFloat("Far plane", &s_far, 100.0f, 100000.0f);

        if (ImGui::Button("Reset near"))
        {
            s_near = 0.1f;
        }
        if (ImGui::Button("Reset far"))
        {
            s_far = 200.0f;
        }

        ImGui::SliderFloat("Sun illuminance", &s_sun_illuminance, 0.1f, 100.f);
        ImGui::SliderFloat("Multiple scattering", &s_multiple_scattering, 0.0f, 1.0f);
        r.p_ui->end_window();
    }
    globals->sun_illuminance     = s_sun_illuminance * float3(1.0f);

    api.end_label();
}

/// --- Where the magic happens

void Renderer::draw()
{
    imgui_update(*this);

    bool is_ok = api.start_frame();
    if (!is_ok) {
        ImGui::EndFrame();
        return;
    }

    // do stuff that doesnt use swapchain image
    update_uniforms(*this);

    if (!api.start_present()) {
        ImGui::EndFrame();
        return;
    }

    graph.clear(); // start_frame() ?

    add_floor_pass(*this);

    ImGui::EndFrame(); // right before drawing the ui
    add_imgui_pass(*this);

    graph.output = graph.swapchain;

    graph.execute();

    // graph.end_frame() ?

    api.end_frame();
}

} // namespace my_app
