#include "app.hpp"
#include "camera.hpp"
#include "imgui/imgui.h"
#include "renderer/hl_api.hpp"
#include "renderer/renderer.hpp"
#include "tools.hpp"
#include "timer.hpp"
#include <vulkan/vulkan_core.h>
#include <cstring> // for std::memcpy
#include "../shaders/include/atmosphere.h"

namespace my_app
{

Renderer::CheckerBoardFloorPass create_floor_pass(vulkan::API &api);
Renderer::ImGuiPass create_imgui_pass(vulkan::API &api);
Renderer::ProceduralSkyPass create_procedural_sky_pass(vulkan::API &api);
Renderer::TonemappingPass create_tonemapping_pass(vulkan::API &api);

Renderer Renderer::create(const Window &window, Camera &camera, TimerData &timer, UI::Context &ui)
{
    // where to put this code?

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

    //

    Renderer r;
    r.api   = vulkan::API::create(window);
    r.graph = RenderGraph::create(r.api);

    r.p_ui     = &ui;
    r.p_window = &window;
    r.p_camera = &camera;
    r.p_timer  = &timer;

    r.imgui              = create_imgui_pass(r.api);
    r.checkerboard_floor = create_floor_pass(r.api);
    r.procedural_sky     = create_procedural_sky_pass(r.api);
    r.tonemapping        = create_tonemapping_pass(r.api);

    // it would be nice to be able to create those in the create_procedural_sky_pass function

    r.transmittance_lut = r.graph.image_descs.add({
        .name      = "Transmittance LUT",
        .size_type = SizeType::Absolute,
        .size      = float3(256, 64, 1),
        .format    = VK_FORMAT_R16G16B16A16_SFLOAT,
    });

    r.skyview_lut = r.graph.image_descs.add({
        .name      = "Skyview LUT",
        .size_type = SizeType::Absolute,
        .size      = float3(192, 108, 1),
        .format    = VK_FORMAT_R16G16B16A16_SFLOAT,
    });

    r.multiscattering_lut = r.graph.image_descs.add({
        .name      = "Multiscattering LUT",
        .size_type = SizeType::Absolute,
        .size      = float3(32, 32, 1),
        .format    = VK_FORMAT_R16G16B16A16_SFLOAT,
    });

    // basic resources

    r.depth_buffer = r.graph.image_descs.add({.name = "Depth Buffer", .format = VK_FORMAT_D32_SFLOAT});
    r.hdr_buffer   = r.graph.image_descs.add({.name = "HDR Buffer", .format = VK_FORMAT_R16G16B16A16_SFLOAT});

    r.trilinear_sampler = r.api.create_sampler({.mag_filter   = VK_FILTER_LINEAR,
                                                .min_filter   = VK_FILTER_LINEAR,
                                                .mip_map_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR});

    r.nearest_sampler = r.api.create_sampler({.mag_filter   = VK_FILTER_NEAREST,
                                              .min_filter   = VK_FILTER_NEAREST,
                                              .mip_map_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST});

    return r;
}

void Renderer::destroy()
{
    api.wait_idle();
    api.destroy();

    ImGui::DestroyContext();
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
        .color_attachment = r.hdr_buffer,
        .depth_attachment = r.depth_buffer,
        .exec = [pass_data=r.checkerboard_floor, global_data=r.global_uniform_pos](RenderGraph& /*graph*/, RenderPass &/*self*/, vulkan::API &api)
        {
            auto program = pass_data.program;

            api.bind_buffer(program, vulkan::GLOBAL_DESCRIPTOR_SET, 0, global_data);
            api.bind_program(program);
            api.bind_index_buffer(pass_data.index_buffer);
            api.bind_vertex_buffer(pass_data.vertex_buffer);

            api.draw_indexed(6, 1, 0, 0, 0);
        }
    });
}

/// --- ImGui Pass


Renderer::ImGuiPass create_imgui_pass(vulkan::API &api)
{
    Renderer::ImGuiPass pass;

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

/// --- Procedure sky

Renderer::ProceduralSkyPass create_procedural_sky_pass(vulkan::API &api)
{
    Renderer::ProceduralSkyPass pass;

    {
        vulkan::GraphicsProgramInfo pinfo{};
        pinfo.vertex_shader   = api.create_shader("shaders/fullscreen_triangle.vert.spv");
        pinfo.fragment_shader = api.create_shader("shaders/transmittance_lut.frag.spv");

        pinfo.binding({
            .set    = vulkan::GLOBAL_DESCRIPTOR_SET,
            .slot   = 0,
            .stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
            .type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        });

        pinfo.binding({
            .set    = vulkan::SHADER_DESCRIPTOR_SET,
            .slot   = 0,
            .stages = VK_SHADER_STAGE_FRAGMENT_BIT,
            .type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        });

        pass.render_transmittance = api.create_program(std::move(pinfo));
    }

    {
        vulkan::GraphicsProgramInfo pinfo{};
        pinfo.vertex_shader   = api.create_shader("shaders/fullscreen_triangle.vert.spv");
        pinfo.fragment_shader = api.create_shader("shaders/skyview_lut.frag.spv");

        // globla uniform
        pinfo.binding({
            .set    = vulkan::GLOBAL_DESCRIPTOR_SET,
            .slot   = 0,
            .stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
            .type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        });

        // atmosphere params
        pinfo.binding({.set    = vulkan::SHADER_DESCRIPTOR_SET,
                       .slot   = 0,
                       .stages = VK_SHADER_STAGE_FRAGMENT_BIT,
                       .type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC});

        // transmittance LUT
        pinfo.binding({.set    = vulkan::SHADER_DESCRIPTOR_SET,
                       .slot   = 1,
                       .stages = VK_SHADER_STAGE_FRAGMENT_BIT,
                       .type   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER});

        // multiscattering LUT
        pinfo.binding({.set    = vulkan::SHADER_DESCRIPTOR_SET,
                       .slot   = 2,
                       .stages = VK_SHADER_STAGE_FRAGMENT_BIT,
                       .type   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER});

        pass.render_skyview = api.create_program(std::move(pinfo));
    }

    {
        vulkan::GraphicsProgramInfo pinfo{};
        pinfo.vertex_shader   = api.create_shader("shaders/fullscreen_triangle.vert.spv");
        pinfo.fragment_shader = api.create_shader("shaders/sky_raymarch.frag.spv");

        pinfo.binding({
            .set    = vulkan::GLOBAL_DESCRIPTOR_SET,
            .slot   = 0,
            .stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
            .type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        });

        // atmosphere params
        pinfo.binding({.set    = vulkan::SHADER_DESCRIPTOR_SET,
                       .slot   = 0,
                       .stages = VK_SHADER_STAGE_FRAGMENT_BIT,
                       .type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC});

        // transmittance lut
        pinfo.binding({.set    = vulkan::SHADER_DESCRIPTOR_SET,
                       .slot   = 1,
                       .stages = VK_SHADER_STAGE_FRAGMENT_BIT,
                       .type   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER});

        // skyview lut
        pinfo.binding({.set    = vulkan::SHADER_DESCRIPTOR_SET,
                       .slot   = 2,
                       .stages = VK_SHADER_STAGE_FRAGMENT_BIT,
                       .type   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER});

        // depth
        pinfo.binding({.set    = vulkan::SHADER_DESCRIPTOR_SET,
                       .slot   = 3,
                       .stages = VK_SHADER_STAGE_FRAGMENT_BIT,
                       .type   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER});

        // multiscattering
        pinfo.binding({.set    = vulkan::SHADER_DESCRIPTOR_SET,
                       .slot   = 4,
                       .stages = VK_SHADER_STAGE_FRAGMENT_BIT,
                       .type   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER});

        pass.sky_raymarch = api.create_program(std::move(pinfo));
    }

    {
        vulkan::ComputeProgramInfo pinfo{};
        pinfo.shader = api.create_shader("shaders/multiscat_lut.comp.spv");
        // atmosphere params
        pinfo.binding({.slot =  0,
                       .stages =  VK_SHADER_STAGE_COMPUTE_BIT,
                       .type =  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC});

        // transmittance lut
        pinfo.binding({.slot =  1,
                       .stages =  VK_SHADER_STAGE_COMPUTE_BIT,
                       .type =  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER});

        // multiscattering lut
        pinfo.binding({.slot =  2,
                       .stages =  VK_SHADER_STAGE_COMPUTE_BIT,
                       .type =  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE});

        pass.compute_multiscattering_lut = api.create_program(std::move(pinfo));
    }

    return pass;
}

void add_procedural_sky_pass(Renderer &r)
{
    auto &api = r.api;
    auto &graph = r.graph;

    assert_uniform_size(AtmosphereParameters);
    static_assert(sizeof(AtmosphereParameters) == 240);

    r.procedural_sky.atmosphere_params_pos = api.dynamic_uniform_buffer(sizeof(AtmosphereParameters));
    auto *p = reinterpret_cast<AtmosphereParameters *>(r.procedural_sky.atmosphere_params_pos.mapped);

    {
        // info.solar_irradiance = { 1.474000f, 1.850400f, 1.911980f };
        p->solar_irradiance = {1.0f, 1.0f, 1.0f}; // Using a normalise sun illuminance. This is to make sure the LUTs acts as a transfert
                                                  // factor to apply the runtime computed sun irradiance over.
        p->sun_angular_radius = 0.004675f;

        // Earth
        p->bottom_radius = 6360000.0f;
        p->top_radius    = 6460000.0f;
        p->ground_albedo = {0.0f, 0.0f, 0.0f};

        // Raleigh scattering
        constexpr double kRayleighScaleHeight = 8000.0;
        constexpr double kMieScaleHeight      = 1200.0;

        p->rayleigh_density.width     = 0.0f;
        p->rayleigh_density.layers[0] = DensityProfileLayer{.exp_term = 1.0f, .exp_scale = -1.0f / kRayleighScaleHeight};
        p->rayleigh_scattering        = {0.000005802f, 0.000013558f, 0.000033100f};

        // Mie scattering
        p->mie_density.width  = 0.0f;
        p->mie_density.layers[0] = DensityProfileLayer{.exp_term = 1.0f, .exp_scale = -1.0f / kMieScaleHeight};

        p->mie_scattering        = {0.000003996f, 0.000003996f, 0.000003996f};
        p->mie_extinction        = {0.000004440f, 0.000004440f, 0.000004440f};
        p->mie_phase_function_g  = 0.8f;

        // Ozone absorption
        p->absorption_density.width  = 25000.0f;
        p->absorption_density.layers[0] = DensityProfileLayer{.linear_term =  1.0f / 15000.0f, .constant_term = -2.0f / 3.0f};

        p->absorption_density.layers[1] = DensityProfileLayer{.linear_term = -1.0f / 15000.0f, .constant_term =  8.0f / 3.0f};
        p->absorption_extinction      = {0.000000650f, 0.000001881f, 0.000000085f};

        const double max_sun_zenith_angle = PI * 120.0 / 180.0; // (use_half_precision_ ? 102.0 : 120.0) / 180.0 * kPi;
        p->mu_s_min                       = (float)cos(max_sun_zenith_angle);
    }

    graph.add_pass({
        .name             = "Transmittance LUT",
        .type             = PassType::Graphics,
        .color_attachment = r.transmittance_lut,
        .exec =
            [pass_data   = r.procedural_sky,
             global_data = r.global_uniform_pos](RenderGraph & /*graph*/, RenderPass & /*self*/, vulkan::API &api) {
                auto program = pass_data.render_transmittance;

                api.bind_buffer(program, vulkan::GLOBAL_DESCRIPTOR_SET, 0, global_data);
                api.bind_buffer(program, vulkan::SHADER_DESCRIPTOR_SET, 0, pass_data.atmosphere_params_pos);
                api.bind_program(program);

                api.draw(3, 1, 0, 0);
            },
    });

    // TODO: compute exec and sampled/storage images barriers
    graph.add_pass({
        .name                    = "Sky Multiscattering LUT",
        .type                    = PassType::Compute,
        .sampled_images = {r.transmittance_lut},
        .storage_images          = {r.multiscattering_lut},
        .exec =
            [pass_data         = r.procedural_sky,
             trilinear_sampler = r.trilinear_sampler](RenderGraph &graph, RenderPass &self, vulkan::API &api) {
                auto transmittance   = graph.get_resolved_image(self.sampled_images[0]);
                auto multiscattering = graph.get_resolved_image(self.storage_images[0]);
                auto program         = pass_data.compute_multiscattering_lut;

                api.bind_buffer(program, 0, pass_data.atmosphere_params_pos);
                api.bind_combined_image_sampler(program, 1, transmittance, trilinear_sampler);
                api.bind_image(program, 2, multiscattering);

                auto multiscattering_desc = *graph.image_descs.get(self.storage_images[0]);
                auto size_x               = static_cast<uint>(multiscattering_desc.size.x);
                auto size_y               = static_cast<uint>(multiscattering_desc.size.y);
                api.dispatch(program, size_x, size_y, 1);
            },
    });

    graph.add_pass({
        .name                    = "Skyview LUT",
        .type                    = PassType::Graphics,
        .sampled_images = {r.transmittance_lut, r.multiscattering_lut},
        .color_attachment        = r.skyview_lut,
        .exec =
            [pass_data         = r.procedural_sky,
             global_data       = r.global_uniform_pos,
             trilinear_sampler = r.trilinear_sampler](RenderGraph &graph, RenderPass &self, vulkan::API &api) {
                auto transmittance   = graph.get_resolved_image(self.sampled_images[0]);
                auto multiscattering = graph.get_resolved_image(self.sampled_images[1]);
                auto program         = pass_data.render_skyview;

                api.bind_buffer(program, vulkan::GLOBAL_DESCRIPTOR_SET, 0, global_data);
                api.bind_buffer(program, vulkan::SHADER_DESCRIPTOR_SET, 0, pass_data.atmosphere_params_pos);

                api.bind_combined_image_sampler(program,
                                                vulkan::SHADER_DESCRIPTOR_SET,
                                                1,
                                                transmittance,
                                                trilinear_sampler);

                api.bind_combined_image_sampler(program,
                                                vulkan::SHADER_DESCRIPTOR_SET,
                                                2,
                                                multiscattering,
                                                trilinear_sampler);

                api.bind_program(program);

                api.draw(3, 1, 0, 0);
            },
    });

    graph.add_pass({
        .name                    = "Sky raymarch",
        .type                    = PassType::Graphics,
        .sampled_images = {r.transmittance_lut, r.multiscattering_lut, r.depth_buffer, r.skyview_lut},
        .color_attachment        = r.hdr_buffer,
        .exec =
            [pass_data         = r.procedural_sky,
             global_data       = r.global_uniform_pos,
             trilinear_sampler = r.trilinear_sampler,
             nearest_sampler   = r.nearest_sampler](RenderGraph &graph, RenderPass &self, vulkan::API &api) {
                auto transmittance   = graph.get_resolved_image(self.sampled_images[0]);
                auto multiscattering = graph.get_resolved_image(self.sampled_images[1]);
                auto depth           = graph.get_resolved_image(self.sampled_images[2]);
                auto skyview         = graph.get_resolved_image(self.sampled_images[3]);
                auto program         = pass_data.sky_raymarch;

                api.bind_buffer(program, vulkan::GLOBAL_DESCRIPTOR_SET, 0, global_data);
                api.bind_buffer(program, vulkan::SHADER_DESCRIPTOR_SET, 0, pass_data.atmosphere_params_pos);

                api.bind_combined_image_sampler(program,
                                                vulkan::SHADER_DESCRIPTOR_SET,
                                                1,
                                                transmittance,
                                                trilinear_sampler);

                api.bind_combined_image_sampler(program, vulkan::SHADER_DESCRIPTOR_SET, 2, skyview, trilinear_sampler);

                api.bind_combined_image_sampler(program, vulkan::SHADER_DESCRIPTOR_SET, 3, depth, nearest_sampler);

                api.bind_combined_image_sampler(program,
                                                vulkan::SHADER_DESCRIPTOR_SET,
                                                4,
                                                multiscattering,
                                                trilinear_sampler);

                api.bind_program(program);

                api.draw(3, 1, 0, 0);
            },
    });
}

/// --- Tonemapping

Renderer::TonemappingPass create_tonemapping_pass(vulkan::API &api)
{
    vulkan::GraphicsProgramInfo pinfo{};
    pinfo.vertex_shader   = api.create_shader("shaders/fullscreen_triangle.vert.spv");
    pinfo.fragment_shader = api.create_shader("shaders/hdr_compositing.frag.spv");

    pinfo.binding({.set    = vulkan::SHADER_DESCRIPTOR_SET,
                   .slot   = 0,
                   .stages = VK_SHADER_STAGE_FRAGMENT_BIT,
                   .type   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER});

    pinfo.binding({.set    = vulkan::SHADER_DESCRIPTOR_SET,
                   .slot   = 1,
                   .stages = VK_SHADER_STAGE_FRAGMENT_BIT,
                   .type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC});

    Renderer::TonemappingPass pass;
    pass.program    = api.create_program(std::move(pinfo));
    pass.params_pos = {};
    return pass;
}

void add_tonemapping_pass(Renderer &r)
{
    auto &api   = r.api;
    auto &ui    = *r.p_ui;
    auto &graph = r.graph;

    static usize s_selected = 1;
    static float s_exposure = 1.0f;

    if (ui.begin_window("HDR Shader", true))
    {
        static std::array options{"Reinhard", "Exposure", "Clamp"};
        tools::imgui_select("Tonemap", options.data(), options.size(), s_selected);
        ImGui::SliderFloat("Exposure", &s_exposure, 0.0f, 10.0f);
        ui.end_window();
    }

    // Make a shader debugging window and its own uniform buffer
    {
        r.tonemapping.params_pos   = api.dynamic_uniform_buffer(sizeof(uint) + sizeof(float));
        auto *buffer = reinterpret_cast<uint *>(r.tonemapping.params_pos.mapped);
        buffer[0] = static_cast<uint>(s_selected);
        auto *floatbuffer = reinterpret_cast<float*>(buffer + 1);
        floatbuffer[0] = s_exposure;
    }

    graph.add_pass({
        .name = "Tonemapping",
        .type = PassType::Graphics,
        .sampled_images = { r.hdr_buffer },
        .color_attachment = graph.swapchain,
        .exec = [pass_data=r.tonemapping, default_sampler=r.nearest_sampler](RenderGraph& graph, RenderPass &self, vulkan::API &api)
        {
            auto hdr_buffer = graph.get_resolved_image(self.sampled_images[0]);
            auto program = pass_data.program;

            api.bind_combined_image_sampler(program, vulkan::SHADER_DESCRIPTOR_SET, 0, hdr_buffer, default_sampler);
            api.bind_buffer(program, vulkan::SHADER_DESCRIPTOR_SET, 1, pass_data.params_pos);
            api.bind_program(program);

            api.draw(3, 1, 0, 0);
        }
    });
}

/// --- BIG MESS

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

void Renderer::display_ui(UI::Context &ui)
{
    graph.display_ui(ui);
    api.display_ui(ui);

    ImGuiIO &io  = ImGui::GetIO();
    const auto &window = *p_window;
    auto &timer  = *p_timer;

    io.DeltaTime = timer.get_delta_time();
    io.Framerate = timer.get_average_fps();

    io.DisplaySize.x             = float(api.ctx.swapchain.extent.width);
    io.DisplaySize.y             = float(api.ctx.swapchain.extent.height);
    io.DisplayFramebufferScale.x = window.get_dpi_scale().x;
    io.DisplayFramebufferScale.y = window.get_dpi_scale().y;

    if (ui.begin_window("Profiler", true))
    {
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
            ImGui::Text("%7.1f", double(timer.get_average_fps()));

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
            ImGui::Text("%9.3f", double(timer.get_average_delta_time()));

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

        const auto &timestamps = api.timestamps;
        if (!timestamps.empty())
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

void Renderer::draw()
{
    bool is_ok = api.start_frame();
    if (!is_ok) {
        ImGui::EndFrame();
        return;
    }
    graph.clear(); // start_frame() ?

    // do stuff that doesnt use swapchain image
    update_uniforms(*this);
    add_floor_pass(*this);
    add_procedural_sky_pass(*this);

    if (!api.start_present()) {
        ImGui::EndFrame();
        return;
    }

    add_tonemapping_pass(*this);

    ImGui::EndFrame(); // right before drawing the ui
    add_imgui_pass(*this);

    graph.execute();

    // graph.end_frame() ?

    api.end_frame();
}

} // namespace my_app
