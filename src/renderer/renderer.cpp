#include "base/types.hpp"
#include "../shaders/include/atmosphere.h"
#include "app.hpp"
#include "camera.hpp"
#include "gltf.hpp"
#include "imgui/imgui.h"
#include "renderer/hl_api.hpp"
#include "renderer/renderer.hpp"
#include "timer.hpp"
#include "tools.hpp"

#include <algorithm>
#include <cstring> // for std::memcpy
#include <future>


#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <vulkan/vulkan_core.h>

namespace my_app
{

Renderer::CheckerBoardFloorPass create_floor_pass(vulkan::API &api);
Renderer::ImGuiPass create_imgui_pass(vulkan::API &api);
Renderer::ProceduralSkyPass create_procedural_sky_pass(vulkan::API &api);
Renderer::TonemappingPass create_tonemapping_pass(vulkan::API &api);
Renderer::GltfPass create_gltf_pass(vulkan::API &api, std::shared_ptr<Model> &model);
Renderer::VoxelPass create_voxel_pass(vulkan::API &/*api*/);

// frame data
void Renderer::create(Renderer& r, const window::Window &window, Camera &camera, TimerData &timer, UI::Context &ui)
{
    // where to put this code?
    //

    vulkan::API::create(r.api, window);
    RenderGraph::create(r.graph, r.api);
    r.model = std::make_shared<Model>(load_model("../models/SponzaBlender/SponzaBlender.gltf")); // TODO: where??

    r.p_ui     = &ui;
    r.p_camera = &camera;
    r.p_timer  = &timer;

    r.api.global_bindings.binding({
        .set    = vulkan::GLOBAL_DESCRIPTOR_SET,
        .slot   = 0,
        .stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |  VK_SHADER_STAGE_COMPUTE_BIT,
        .type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
    });

    // bindless binding model, most textures are bound only once per frame
    // - per frame/pass textures are hardcoded at the beginning of the set (immediates are 20 bit on AMD)
    // - per draw textures are accessed with (push constant index + 1)
    // (https://gpuopen.com/wp-content/uploads/2016/03/VulkanFastPaths.pdf)
    r.api.global_bindings.binding({
        .set    = vulkan::GLOBAL_DESCRIPTOR_SET,
        .slot   = 1,
        .stages = VK_SHADER_STAGE_FRAGMENT_BIT,
        .type   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .count  = 128,
    });

    r.api.create_global_set();

    r.imgui              = create_imgui_pass(r.api);
    r.checkerboard_floor = create_floor_pass(r.api);
    r.procedural_sky     = create_procedural_sky_pass(r.api);
    r.tonemapping        = create_tonemapping_pass(r.api);
    r.gltf               = create_gltf_pass(r.api, r.model);
    r.voxels             = create_voxel_pass(r.api);

    // basic resources

    r.settings = {};
    auto resolution_scale = r.settings.resolution_scale;
    r.depth_buffer     = r.graph.image_descs.add({.name   = "Depth Buffer",
                                              .size   = float3(resolution_scale, resolution_scale, 1.0f),
                                              .format = VK_FORMAT_D32_SFLOAT});
    r.hdr_buffer       = r.graph.image_descs.add({.name   = "HDR Buffer",
                                            .size   = float3(resolution_scale, resolution_scale, 1.0f),
                                            .format = VK_FORMAT_R16G16B16A16_SFLOAT});
    r.ldr_buffer       = r.graph.image_descs.add({.name   = "LDR Buffer",
                                            .size   = float3(resolution_scale, resolution_scale, 1.0f),
                                            .format = r.api.ctx.swapchain.format.format});

    r.trilinear_sampler = r.api.create_sampler({.mag_filter   = VK_FILTER_LINEAR,
                                                .min_filter   = VK_FILTER_LINEAR,
                                                .mip_map_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR});

    r.nearest_sampler = r.api.create_sampler({.mag_filter   = VK_FILTER_NEAREST,
                                              .min_filter   = VK_FILTER_NEAREST,
                                              .mip_map_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST});

    float aspect_ratio = r.api.ctx.swapchain.extent.width / float(r.api.ctx.swapchain.extent.height);
    r.p_camera->near_plane = 0.4f;
    r.p_camera->far_plane = 1000.0f;
    r.p_camera->projection = Camera::perspective(60.0f, aspect_ratio, r.p_camera->near_plane, r.p_camera->far_plane, &r.p_camera->projection_inverse);
    // r.p_camera->update_view();

    r.sun.position = float3(0.0f, 40.0f, 0.0f);
    r.sun.pitch    = 0.0f;
    r.sun.yaw      = 25.0f;
    r.sun.roll     = 80.0f;

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

    r.voxels_albedo = r.graph.image_descs.add({
        .name          = "Voxels albedo",
        .size_type     = SizeType::Absolute,
        .size          = float3(r.voxel_options.res),
        .type          = VK_IMAGE_TYPE_3D,
        .format        = VK_FORMAT_R8G8B8A8_UNORM,
        .extra_formats = {VK_FORMAT_R32_UINT},
    });

    r.voxels_normal = r.graph.image_descs.add({
        .name          = "Voxels normal",
        .size_type     = SizeType::Absolute,
        .size          = float3(r.voxel_options.res),
        .type          = VK_IMAGE_TYPE_3D,
        .format        = VK_FORMAT_R8G8B8A8_UNORM,
        .extra_formats = {VK_FORMAT_R32_UINT},
    });

    r.voxels_radiance = r.graph.image_descs.add({
        .name          = "Voxels radiance",
        .size_type     = SizeType::Absolute,
        .size          = float3(r.voxel_options.res),
        .type          = VK_IMAGE_TYPE_3D,
        .format        = VK_FORMAT_R16G16B16A16_SFLOAT,
    });

    usize name_i = 0;
    std::array names = {
        "Voxels volume -X",
        "Voxels volume +X",
        "Voxels volume -Y",
        "Voxels volume +Y",
        "Voxels volume -Z",
        "Voxels volume +Z"
    };
    for (auto &volume : r.voxels_directional_volumes)
    {
        u32 size = r.voxel_options.res / 2;
        volume = r.graph.image_descs.add({
            .name      = names[name_i++],
            .size_type = SizeType::Absolute,
            .size      = float3(size),
            .type      = VK_IMAGE_TYPE_3D,
            .format    = VK_FORMAT_R16G16B16A16_SFLOAT,
            .levels    = static_cast<u32>(std::floor(std::log2(size)) + 1.0)
        });
    }

    r.shadow_cascades.resize(r.settings.shadow_cascades_count);
    for (auto& shadow_cascade : r.shadow_cascades)
    {
        shadow_cascade = r.graph.image_descs.add({
            .name          = "Shadow cascade",
            .size_type     = SizeType::Absolute,
            .size          = float3(2048.0f, 2048.0f, 1.0f),
            .type          = VK_IMAGE_TYPE_2D,
            .format        = VK_FORMAT_D32_SFLOAT,
        });
    }
}

void Renderer::destroy()
{
    api.wait_idle();
    graph.destroy();
    api.destroy();
}

void Renderer::on_resize(int width, int height)
{
    api.on_resize(width, height);
    graph.on_resize(width, height);
}

void Renderer::wait_idle()
{
    api.wait_idle();
}

void Renderer::reload_shader(std::string_view /*unused*/)
{
}

/// --- Checker board floor

Renderer::CheckerBoardFloorPass create_floor_pass(vulkan::API &api)
{
    Renderer::CheckerBoardFloorPass pass;

    pass.program = api.create_program({
        .vertex_shader   = api.create_shader("shaders/checkerboard_floor.vert.spv"),
        .fragment_shader = api.create_shader("shaders/checkerboard_floor.frag.spv"),
        .depth_test      = VK_COMPARE_OP_GREATER_OR_EQUAL,
    });

    return pass;
}

static void add_floor_pass(Renderer &r)
{
    auto &graph = r.graph;

    graph.add_pass({
        .name = "Checkerboard Floor pass",
        .type = PassType::Graphics,
        .color_attachments = {graph.swapchain},
        .depth_attachment = r.depth_buffer,
        .exec = [pass_data=r.checkerboard_floor](RenderGraph& /*graph*/, RenderPass &/*self*/, vulkan::API &api)
        {
            auto program = pass_data.program;
            api.bind_program(program);
            api.draw(6, 1, 0, 0);
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
    api.transfer_done(pass.font_atlas);
    return pass;
}

static void add_imgui_pass(Renderer &r)
{
    ImGui::Render();
    ImDrawData *data = ImGui::GetDrawData();
    if (data == nullptr || data->TotalVtxCount == 0) {
        return;
    }

    auto &graph = r.graph;
    auto &api = r.api;

    // The render graph needs to know about external images to put barriers on them correctly
    // are external images always going to be sampled or they need to be in differents categories
    // like regular images from the graph?
    std::vector<vulkan::ImageH> external_images;
    external_images.push_back(r.imgui.font_atlas);

    for (int list = 0; list < data->CmdListsCount; list++) {
        const auto &cmd_list = *data->CmdLists[list];

        for (int command_index = 0; command_index < cmd_list.CmdBuffer.Size; command_index++) {
            const auto &draw_command = cmd_list.CmdBuffer[command_index];

            if (draw_command.TextureId) {
                const auto& image_view_h = *reinterpret_cast<const vulkan::ImageViewH*>(&draw_command.TextureId);
                external_images.push_back(api.get_image_view(image_view_h).image_h);
            }
        }
    }

    auto &trilinear_sampler = r.trilinear_sampler;

    graph.add_pass({
        .name = "ImGui pass",
        .type = PassType::Graphics,
        .external_images = external_images,
        .color_attachments = {graph.swapchain},
        .exec = [pass_data = r.imgui, trilinear_sampler](RenderGraph& /*graph*/, RenderPass &/*self*/, vulkan::API &api)
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
            scale_and_translation.raw[0] = 2.0f / data->DisplaySize.x;                            // X Scale
            scale_and_translation.raw[1] = 2.0f / data->DisplaySize.y;                            // Y Scale
            scale_and_translation.raw[2] = -1.0f - data->DisplayPos.x * scale_and_translation.raw[0]; // X Translation
            scale_and_translation.raw[3] = -1.0f - data->DisplayPos.y * scale_and_translation.raw[1]; // Y Translation

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
                    const auto &draw_command = cmd_list.CmdBuffer[command_index];

                    vulkan::GraphicsProgramH current = pass_data.float_program;

                    if (draw_command.TextureId) {
                        const auto& texture = *reinterpret_cast<const vulkan::ImageViewH*>(&draw_command.TextureId);
                        auto& image_view = api.get_image_view(texture);

                        if (image_view.format == VK_FORMAT_R32_UINT)
                        {
                            current = pass_data.uint_program;
                        }

                        api.bind_combined_image_sampler(current, texture, trilinear_sampler, vulkan::SHADER_DESCRIPTOR_SET, 0);
                    }
                    else {
                        api.bind_combined_image_sampler(current, api.get_image(pass_data.font_atlas).default_view, trilinear_sampler, vulkan::SHADER_DESCRIPTOR_SET, 0);
                    }

                    api.bind_program(current);
                    api.push_constant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float4), &scale_and_translation);

                    // Project scissor/clipping rectangles into framebuffer space
                    ImVec4 clip_rect;
                    clip_rect.x = (draw_command.ClipRect.x - clip_off.x) * clip_scale.x;
                    clip_rect.y = (draw_command.ClipRect.y - clip_off.y) * clip_scale.y;
                    clip_rect.z = (draw_command.ClipRect.z - clip_off.x) * clip_scale.x;
                    clip_rect.w = (draw_command.ClipRect.w - clip_off.y) * clip_scale.y;

                    // Apply scissor/clipping rectangle
                    // FIXME: We could clamp width/height based on clamped min/max values.
                    VkRect2D scissor;
                    scissor.offset.x      = (static_cast<i32>(clip_rect.x) > 0) ? static_cast<i32>(clip_rect.x) : 0;
                    scissor.offset.y      = (static_cast<i32>(clip_rect.y) > 0) ? static_cast<i32>(clip_rect.y) : 0;
                    scissor.extent.width  = static_cast<u32>(clip_rect.z - clip_rect.x);
                    scissor.extent.height = static_cast<u32>(clip_rect.w - clip_rect.y + 1); // FIXME: Why +1 here?

                    api.set_scissor(scissor);

                    api.draw_indexed(draw_command.ElemCount, 1, index_offset, vertex_offset, 0);

                    index_offset += draw_command.ElemCount;
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

    pass.render_transmittance = api.create_program({
        .vertex_shader   = api.create_shader("shaders/fullscreen_triangle.vert.spv"),
        .fragment_shader = api.create_shader("shaders/transmittance_lut.frag.spv"),
    });

    pass.render_skyview = api.create_program({
        .vertex_shader   = api.create_shader("shaders/fullscreen_triangle.vert.spv"),
        .fragment_shader = api.create_shader("shaders/skyview_lut.frag.spv"),
    });

    pass.sky_raymarch = api.create_program({
        .vertex_shader   = api.create_shader("shaders/fullscreen_triangle.vert.spv"),
        .fragment_shader = api.create_shader("shaders/sky_raymarch.frag.spv"),
    });

    pass.compute_multiscattering_lut = api.create_program({
        .shader = api.create_shader("shaders/multiscat_lut.comp.spv"),
    });

    return pass;
}

static void add_procedural_sky_pass(Renderer &r)
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
        .color_attachments = {r.transmittance_lut},
        .exec =
            [pass_data   = r.procedural_sky](RenderGraph & /*graph*/, RenderPass & /*self*/, vulkan::API &api) {
                auto program = pass_data.render_transmittance;

                api.bind_buffer(program, pass_data.atmosphere_params_pos, vulkan::SHADER_DESCRIPTOR_SET, 0);
                api.bind_program(program);

                api.draw(3, 1, 0, 0);
            },
    });

    graph.add_pass({
        .name           = "Sky Multiscattering LUT",
        .type           = PassType::Compute,
        .sampled_images = {r.transmittance_lut},
        .storage_images = {r.multiscattering_lut},
        .exec =
            [pass_data         = r.procedural_sky,
             trilinear_sampler = r.trilinear_sampler](RenderGraph &graph, RenderPass &self, vulkan::API &api) {
                auto transmittance   = graph.get_resolved_image(self.sampled_images[0]);
                auto multiscattering = graph.get_resolved_image(self.storage_images[0]);
                auto program         = pass_data.compute_multiscattering_lut;

                api.bind_buffer(program, pass_data.atmosphere_params_pos, 0);
                api.bind_combined_image_sampler(program, api.get_image(transmittance).default_view, trilinear_sampler, 1);
                api.bind_image(program, api.get_image(multiscattering).default_view, 2);

                auto multiscattering_desc = *graph.image_descs.get(self.storage_images[0]);
                auto size_x               = static_cast<uint>(multiscattering_desc.size.x);
                auto size_y               = static_cast<uint>(multiscattering_desc.size.y);
                api.dispatch(program, size_x, size_y, 1);
            },
    });

    graph.add_pass({
        .name             = "Skyview LUT",
        .type             = PassType::Graphics,
        .sampled_images   = {r.transmittance_lut, r.multiscattering_lut},
        .color_attachments = {r.skyview_lut},
        .exec =
            [pass_data         = r.procedural_sky,
             trilinear_sampler = r.trilinear_sampler](RenderGraph &graph, RenderPass &self, vulkan::API &api) {
                auto transmittance   = graph.get_resolved_image(self.sampled_images[0]);
                auto multiscattering = graph.get_resolved_image(self.sampled_images[1]);
                auto program         = pass_data.render_skyview;

                api.bind_buffer(program, pass_data.atmosphere_params_pos, vulkan::SHADER_DESCRIPTOR_SET, 0);

                api.bind_combined_image_sampler(program,
                                                api.get_image(transmittance).default_view,
                                                trilinear_sampler,
                                                vulkan::SHADER_DESCRIPTOR_SET,
                                                1);

                api.bind_combined_image_sampler(program,
                                                api.get_image(multiscattering).default_view,
                                                trilinear_sampler,
                                                vulkan::SHADER_DESCRIPTOR_SET,
                                                2);

                api.bind_program(program);

                api.draw(3, 1, 0, 0);
            },
    });

    graph.add_pass({
        .name             = "Sky raymarch",
        .type             = PassType::Graphics,
        .sampled_images   = {r.transmittance_lut, r.multiscattering_lut, r.depth_buffer, r.skyview_lut},
        .color_attachments = {r.hdr_buffer},
        .exec =
            [pass_data         = r.procedural_sky,
             trilinear_sampler = r.trilinear_sampler,
             nearest_sampler   = r.nearest_sampler](RenderGraph &graph, RenderPass &self, vulkan::API &api) {
                auto transmittance   = graph.get_resolved_image(self.sampled_images[0]);
                auto multiscattering = graph.get_resolved_image(self.sampled_images[1]);
                auto depth           = graph.get_resolved_image(self.sampled_images[2]);
                auto skyview         = graph.get_resolved_image(self.sampled_images[3]);
                auto program         = pass_data.sky_raymarch;

                api.bind_buffer(program, pass_data.atmosphere_params_pos, vulkan::SHADER_DESCRIPTOR_SET, 0);

                api.bind_combined_image_sampler(program,
                                                api.get_image(transmittance).default_view,
                                                trilinear_sampler,
                                                vulkan::SHADER_DESCRIPTOR_SET,
                                                1);

                api.bind_combined_image_sampler(program, api.get_image(skyview).default_view, trilinear_sampler, vulkan::SHADER_DESCRIPTOR_SET, 2);

                api.bind_combined_image_sampler(program, api.get_image(depth).default_view, nearest_sampler, vulkan::SHADER_DESCRIPTOR_SET, 3);

                api.bind_combined_image_sampler(program,
                                                api.get_image(multiscattering).default_view,
                                                trilinear_sampler,
                                                vulkan::SHADER_DESCRIPTOR_SET,
                                                4);

                api.bind_program(program);

                api.draw(3, 1, 0, 0);
            },
    });
}

/// --- Tonemapping

Renderer::TonemappingPass create_tonemapping_pass(vulkan::API &api)
{
    Renderer::TonemappingPass pass;

    pass.program = api.create_program({
        .vertex_shader   = api.create_shader("shaders/fullscreen_triangle.vert.spv"),
        .fragment_shader = api.create_shader("shaders/hdr_compositing.frag.spv"),
    });

    pass.params_pos = {};

    return pass;
}

static void add_tonemapping_pass(Renderer &r)
{
    auto &api   = r.api;
    auto &ui    = *r.p_ui;
    auto &graph = r.graph;

    static uint s_selected = 1;
    static float s_exposure = 1.0f;

    if (ui.begin_window("HDR Shader"))
    {
        static std::array options{"Reinhard", "Exposure", "Clamp"};
        tools::imgui_select("Tonemap", options.data(), options.size(), s_selected);
        ImGui::SliderFloat("Exposure", &s_exposure, 0.0f, 2.0f);
        ui.end_window();
    }

    // Make a shader debugging window and its own uniform buffer
    {
        r.tonemapping.params_pos   = api.dynamic_uniform_buffer(sizeof(uint) + sizeof(float));
        auto *buffer = reinterpret_cast<uint *>(r.tonemapping.params_pos.mapped);
        buffer[0] = s_selected;
        auto *floatbuffer = reinterpret_cast<float*>(buffer + 1);
        floatbuffer[0] = s_exposure;
    }

    graph.add_pass({
        .name = "Tonemapping",
        .type = PassType::Graphics,
        .sampled_images = { r.hdr_buffer },
        .color_attachments = {r.ldr_buffer},
        .exec = [pass_data=r.tonemapping, default_sampler=r.nearest_sampler](RenderGraph& graph, RenderPass &self, vulkan::API &api)
        {
            auto hdr_buffer = graph.get_resolved_image(self.sampled_images[0]);
            auto program = pass_data.program;

            api.bind_combined_image_sampler(program, api.get_image(hdr_buffer).default_view, default_sampler, vulkan::SHADER_DESCRIPTOR_SET, 0);
            api.bind_buffer(program, pass_data.params_pos, vulkan::SHADER_DESCRIPTOR_SET, 1);
            api.bind_program(program);

            api.draw(3, 1, 0, 0);
        }
    });
}

/// --- glTF model pass

Renderer::GltfPass create_gltf_pass(vulkan::API &api, std::shared_ptr<Model> &_model)
{
    Renderer::GltfPass pass;
    pass.model = _model;

    auto &model = *pass.model;

    /// --- TODO Move

    model.nodes_preorder.clear();
    model.nodes_preorder.reserve(model.nodes.size());

    model.cached_transforms.resize(model.nodes.size());

    std::vector<u32> nodes_stack;
    nodes_stack.reserve(model.nodes.size());
    nodes_stack.push_back(model.scene[0]);

    while (!nodes_stack.empty())
    {
        auto node_idx = nodes_stack.back();
        nodes_stack.pop_back();
        auto &node = model.nodes[node_idx];

        node.dirty                        = false;
        auto translation                  = float4x4::identity(); //glm::translate(glm::mat4(1.0f), node.translation);
        auto rotation                     = float4x4::identity(); //glm::mat4(node.rotation);
        auto scale                        = float4x4::identity(); // assume uniform scale
        scale.at(0, 0) = node.scale.x;
        scale.at(1, 1) = node.scale.y;
        scale.at(2, 2) = node.scale.z;
        model.cached_transforms[node_idx] = translation * rotation * scale;

        model.nodes_preorder.push_back(node_idx);

        for (auto child : node.children) {
            nodes_stack.push_back(child);
        }
    }

    /// -- end TODO

    usize vbuffer_size = model.vertices.size() * sizeof(GltfVertex);
    pass.vertex_buffer = api.create_buffer({
        .name  = "glTF Vertex Buffer",
        .size  = vbuffer_size,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    });
    api.upload_buffer(pass.vertex_buffer, model.vertices.data(), vbuffer_size);

    usize ibuffer_size = model.indices.size() * sizeof(u16);
    pass.index_buffer  = api.create_buffer({
        .name  = "glTF Index Buffer",
        .size  = ibuffer_size,
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    });
    api.upload_buffer(pass.index_buffer, model.indices.data(), ibuffer_size);

    /// --- Create samplers
    for (auto &sampler : model.samplers)
    {
        vulkan::SamplerInfo sinfo;

        switch (sampler.mag_filter)
        {
            case Filter::Nearest:
                sinfo.mag_filter = VK_FILTER_NEAREST;
                break;
            case Filter::Linear:
                sinfo.mag_filter = VK_FILTER_LINEAR;
                break;
            default:
                break;
        }

        switch (sampler.min_filter)
        {
            case Filter::Nearest:
                sinfo.min_filter = VK_FILTER_NEAREST;
                break;
            case Filter::Linear:
                sinfo.min_filter = VK_FILTER_LINEAR;
                break;
            default:
                break;
        }

        switch (sampler.wrap_s)
        {
            case Wrap::Repeat:
                sinfo.address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                break;
            case Wrap::ClampToEdge:
                sinfo.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
                break;
            case Wrap::MirroredRepeat:
                sinfo.address_mode = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
                break;
            default:
                break;
        }

        pass.samplers.push_back(api.create_sampler(sinfo));
    }

    /// --- Create images
    struct GltfImageInfo
    {
        int width;
        int height;
        u8 *pixels;
        int nb_comp;
        VkFormat format;
    };
    std::vector<std::future<GltfImageInfo>> images_pixels;
    images_pixels.resize(model.images.size());

    for (usize image_i = 0; image_i < model.images.size(); image_i++)
    {
        const auto &image      = model.images[image_i];
        images_pixels[image_i] = std::async(std::launch::async, [&]() {
            GltfImageInfo info = {};
            info.pixels        = stbi_load_from_memory(image.data.data(),
                                                static_cast<int>(image.data.size()),
                                                &info.width,
                                                &info.height,
                                                &info.nb_comp,
                                                0);

            if (info.nb_comp == 1)
            { // NOLINT
                info.format = VK_FORMAT_R8_UNORM;
            }
            else if (info.nb_comp == 2)
            { // NOLINT
                info.format = VK_FORMAT_R8G8_UNORM;
            }
            else if (info.nb_comp == 3)
            { // NOLINT
                stbi_image_free(info.pixels);
                int wanted_nb_comp = 4;
                info.pixels        = stbi_load_from_memory(image.data.data(),
                                                    static_cast<int>(image.data.size()),
                                                    &info.width,
                                                    &info.height,
                                                    &info.nb_comp,
                                                    wanted_nb_comp);
                info.format        = image.srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
                info.nb_comp       = wanted_nb_comp;
            }
            else if (info.nb_comp == 4)
            { // NOLINT
                info.format = image.srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
            }
            else
            { // NOLINT
                assert(false);
            }

            return info;
        });
    }

    for (usize image_i = 0; image_i < model.images.size(); image_i++)
    {
        auto image_info = images_pixels[image_i].get();

        vulkan::ImageInfo iinfo;
        iinfo.name                = "glTF image";
        iinfo.width               = static_cast<u32>(image_info.width);
        iinfo.height              = static_cast<u32>(image_info.height);
        iinfo.depth               = 1;
        iinfo.format              = image_info.format;
        iinfo.generate_mip_levels = true;

        auto image_h = api.create_image(iinfo);
        pass.images.push_back(image_h);

        auto size = static_cast<usize>(image_info.width * image_info.height * image_info.nb_comp);
        api.upload_image(image_h, image_info.pixels, size);
        api.generate_mipmaps(image_h);
        api.transfer_done(image_h);

        stbi_image_free(image_info.pixels);
    }

    /// --- Create programs
    {
        vulkan::GraphicsProgramInfo pinfo{};
        pinfo.vertex_shader   = api.create_shader("shaders/gltf.vert.spv");
        pinfo.fragment_shader = api.create_shader("shaders/gltf.frag.spv");

        pinfo.vertex_stride(sizeof(GltfVertex));
        pinfo.vertex_info({.format = VK_FORMAT_R32G32B32_SFLOAT, .offset = MEMBER_OFFSET(GltfVertex, position)});
        pinfo.vertex_info({.format = VK_FORMAT_R32G32B32_SFLOAT, .offset = MEMBER_OFFSET(GltfVertex, normal)});
        pinfo.vertex_info({.format = VK_FORMAT_R32G32_SFLOAT, .offset = MEMBER_OFFSET(GltfVertex, uv0)});
        pinfo.vertex_info({.format = VK_FORMAT_R32G32_SFLOAT, .offset = MEMBER_OFFSET(GltfVertex, uv1)});
        pinfo.vertex_info({.format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = MEMBER_OFFSET(GltfVertex, joint0)});
        pinfo.vertex_info({.format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = MEMBER_OFFSET(GltfVertex, weight0)});

        pinfo.depth_test         = VK_COMPARE_OP_EQUAL; // equal because depth prepass
        pinfo.enable_depth_write = false;

        pass.shading = api.create_program(std::move(pinfo));
    }

    {
        vulkan::GraphicsProgramInfo pinfo{};
        pinfo.vertex_shader   = api.create_shader("shaders/gltf.vert.spv");
        pinfo.fragment_shader = api.create_shader("shaders/gltf_prepass.frag.spv");

        pinfo.vertex_stride(sizeof(GltfVertex));
        pinfo.vertex_info({VK_FORMAT_R32G32B32_SFLOAT, MEMBER_OFFSET(GltfVertex, position)});
        pinfo.vertex_info({VK_FORMAT_R32G32B32_SFLOAT, MEMBER_OFFSET(GltfVertex, normal)});
        pinfo.vertex_info({VK_FORMAT_R32G32_SFLOAT, MEMBER_OFFSET(GltfVertex, uv0)});
        pinfo.vertex_info({VK_FORMAT_R32G32_SFLOAT, MEMBER_OFFSET(GltfVertex, uv1)});
        pinfo.vertex_info({VK_FORMAT_R32G32B32A32_SFLOAT, MEMBER_OFFSET(GltfVertex, joint0)});
        pinfo.vertex_info({VK_FORMAT_R32G32B32A32_SFLOAT, MEMBER_OFFSET(GltfVertex, weight0)});

        pinfo.depth_test         = VK_COMPARE_OP_GREATER_OR_EQUAL;
        pinfo.enable_depth_write = true;

        pass.prepass = api.create_program(std::move(pinfo));
    }

    {
        vulkan::GraphicsProgramInfo pinfo{};
        pinfo.vertex_shader   = api.create_shader("shaders/shadowmap.vert.spv");
        pinfo.fragment_shader = api.create_shader("shaders/shadowmap.frag.spv");

        pinfo.vertex_stride(sizeof(GltfVertex));
        pinfo.vertex_info({VK_FORMAT_R32G32B32_SFLOAT, MEMBER_OFFSET(GltfVertex, position)});
        pinfo.vertex_info({VK_FORMAT_R32G32B32_SFLOAT, MEMBER_OFFSET(GltfVertex, normal)});
        pinfo.vertex_info({VK_FORMAT_R32G32_SFLOAT, MEMBER_OFFSET(GltfVertex, uv0)});
        pinfo.vertex_info({VK_FORMAT_R32G32_SFLOAT, MEMBER_OFFSET(GltfVertex, uv1)});
        pinfo.vertex_info({VK_FORMAT_R32G32B32A32_SFLOAT, MEMBER_OFFSET(GltfVertex, joint0)});
        pinfo.vertex_info({VK_FORMAT_R32G32B32A32_SFLOAT, MEMBER_OFFSET(GltfVertex, weight0)});

        pinfo.depth_test         = VK_COMPARE_OP_GREATER_OR_EQUAL;
        pinfo.enable_depth_write = true;

        pass.shadow_cascade_program = api.create_program(std::move(pinfo));
    }

    return pass;

}

static void draw_model(vulkan::API &api, Model &model, vulkan::GraphicsProgramH program)
{
    // Bind the node transforms
    auto transforms_pos   = api.dynamic_uniform_buffer(model.nodes.size() * sizeof(float4x4));
    auto *buffer = reinterpret_cast<float4x4 *>(transforms_pos.mapped);
    for (uint i = 0; i < model.cached_transforms.size(); i++) {
        buffer[i]      = model.cached_transforms[i];
    }
    api.bind_buffer(program, transforms_pos, vulkan::SHADER_DESCRIPTOR_SET, 0);

    api.bind_program(program);

    for (auto node_idx : model.nodes_preorder)
    {
        const auto &node = model.nodes[node_idx];
        const auto &mesh = model.meshes[node.mesh];

        // Draw the mesh
        for (const auto &primitive : mesh.primitives)
        {
            const auto &material = model.materials[primitive.material];

            GltfPushConstant constants = {};
            constants.node_idx          = node_idx;

            constants.base_color_idx         = material.base_color_texture ? *material.base_color_texture : u32_invalid;
            constants.normal_map_idx         = material.normal_texture ? *material.normal_texture : u32_invalid;
            constants.metallic_roughness_idx = material.metallic_roughness_texture ? *material.metallic_roughness_texture : u32_invalid;

            constants.base_color_factor = material.base_color_factor;
            constants.metallic_factor   = material.metallic_factor;
            constants.roughness_factor  = material.roughness_factor;

            api.push_constant(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                              0,
                              sizeof(constants),
                              &constants);
            api.draw_indexed(primitive.index_count, 1, primitive.first_index, static_cast<i32>(primitive.first_vertex), 0);
        }
    }
}

static void add_shadow_cascades_pass(Renderer &r)
{
    auto &graph = r.graph;
    auto &api = r.api;
    auto &ui = *r.p_ui;

    auto external_images = r.gltf.images;

    auto cascades_count = r.settings.shadow_cascades_count;

    auto &depth_slices = r.depth_slices;
    auto &cascades_view = r.cascades_view;
    auto &cascades_proj = r.cascades_proj;
    depth_slices.resize(cascades_count);
    cascades_view.resize(cascades_count);
    cascades_proj.resize(cascades_count);

    static float split_factor = 1.0f;
    float near_plane  = r.p_camera->far_plane;
    float far_plane   = r.p_camera->near_plane;

    if (ui.begin_window("Settings", true))
    {
        ImGui::SliderFloat("split", &split_factor, 0.0f, 1.f);
        ui.end_window();
    }

    float clip_range = far_plane - near_plane;
    // "practical split scheme" by nvidia (https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-10-parallel-split-shadow-maps-programmable-gpus)
    // basically a mix of log and uniform splits by a split factor
    for (uint i = 0; i < cascades_count; i++)
    {
        float p = (i+1) / static_cast<float>(cascades_count);
        float log = near_plane * std::pow(far_plane/near_plane, p);
        float uniform = near_plane + clip_range * p;
        float d = split_factor * (log - uniform) + uniform;
        depth_slices[i] = 1.0f - (d - near_plane) / clip_range;
    }

    // compute view and projection  matrix for each cascade
    float last_split = 1.0f;
    for (uint i = 0; i < cascades_count; i++)
    {
        float split = depth_slices[i];

        // get the frustum of the current split (near plane = 1.0f - last_split | far plane = 1.0f - split)
        // it's a bit weird because the splits are in [0, 1] so to use reversed depth there is a 1 - depth
        std::array frustum_corners{
            float3(-1.0f, 1.0f, last_split),
            float3(1.0f, 1.0f, last_split),
            float3(1.0f, -1.0f, last_split),
            float3(-1.0f, -1.0f, last_split),
            float3(-1.0f, 1.0f, split),
            float3(1.0f, 1.0f, split),
            float3(1.0f, -1.0f, split),
            float3(-1.0f, -1.0f, split),
        };

        // project frustum corners into world space
        auto cam_inv_view_pro = r.p_camera->get_inverse_view() * r.p_camera->get_inverse_projection();
        for (uint i = 0; i < 8; i++)
        {
            float4 world_space_corner = cam_inv_view_pro * float4(frustum_corners[i], 1.0f);
            frustum_corners[i]   = (1.0f / world_space_corner.w) * world_space_corner.xyz();
        }

        // get frustum center
        auto center = float3(0.0f);
        for (uint i = 0; i < 8; i++) {
            center = center + frustum_corners[i];
        }
        center = (1.0f / 8.0f) * center;

        // get the radius of the frustum
        float radius = 0.0f;
        for (uint i = 0; i < 8; i++)
        {
            float distance = (frustum_corners[i] - center).norm();
            radius         = std::max(radius, distance);
        }
        radius = std::ceil(radius * 16.0f) / 16.0f;

        float3 light_dir = normalize(r.sun.front);
        auto max   = float3(radius);
        float3 min = -1.0f * max;

        cascades_view[i] = Camera::look_at(center - light_dir * max.z, center, float3(0.0f, 1.0f, 0.0f)); // todo handle the case when light_dir and up are parallel

        // reverse depth
        min.z = (max.z - min.z);
        max.z = 0.0f;
        cascades_proj[i] = Camera::ortho(min, max);

        float4x4 matrix = cascades_view[i] * cascades_proj[i];
        float4 origin = float4(0.0f, 0.0f, 0.0f, 1.0f);
        origin = matrix * origin;
        origin = (2048.f / 2.0f) * origin;

        float4 rounded_origin = round(origin);
        float4 rounded_offset = rounded_origin - origin;
        rounded_offset =  (2.0f / 2048.f) * rounded_offset;
        rounded_offset.z = 0.0f;
        rounded_offset.w = 0.0f;

        float4x4 proj = cascades_proj[i];
        proj.at(0, 3) += rounded_offset.x;
        proj.at(1, 3) += rounded_offset.y;
        proj.at(2, 3) += rounded_offset.z;
        proj.at(3, 3) += rounded_offset.w;
        cascades_proj[i] = proj;

        last_split = split;
    }

    r.matrices_pos = api.dynamic_uniform_buffer(2 * sizeof(float4x4) * cascades_count);
    auto *matrices    = reinterpret_cast<float4x4 *>(r.matrices_pos.mapped);
    for (uint i = 0; i < cascades_count; i++)
    {
        matrices[2 * i]     = cascades_view[i];
        matrices[2 * i + 1] = cascades_proj[i];
    }

    // slices are sent as-is and need to be reversed in shader
    r.depth_slices_pos = r.api.dynamic_uniform_buffer(sizeof(float4) * (r.shadow_cascades.size() + 3) / 4);
    auto *slices    = reinterpret_cast<float *>(r.depth_slices_pos.mapped);
    for (uint i = 0; i < r.shadow_cascades.size(); i++) {
        slices[i]     = r.depth_slices[i];
    }

    for (uint i = 0; i < cascades_count; i++)
    {
        auto cascade_index_pos = api.dynamic_uniform_buffer(sizeof(uint));
        auto *cascade_index    = reinterpret_cast<uint *>(cascade_index_pos.mapped);
        *cascade_index         = i;

        graph.add_pass({.name             = "Shadow Cascade",
                        .type             = PassType::Graphics,
                        .external_images  = external_images,
                        .depth_attachment = r.shadow_cascades[i],
                        .exec             = [pass_data   = r.gltf,
                                 cascade_index_pos,
                                 matrices_pos=r.matrices_pos](RenderGraph & /*graph*/, RenderPass & /*self*/, vulkan::API &api) {
                            // draw glTF
                            {
                                auto program = pass_data.shadow_cascade_program;

                                api.bind_buffer(program, cascade_index_pos, vulkan::SHADER_DESCRIPTOR_SET, 1);
                                api.bind_buffer(program, matrices_pos, vulkan::SHADER_DESCRIPTOR_SET, 2);
                                api.bind_index_buffer(pass_data.index_buffer);
                                api.bind_vertex_buffer(pass_data.vertex_buffer);

                                draw_model(api, *pass_data.model, program);
                            }
                        }});
    }
}

static void add_gltf_prepass(Renderer &r)
{
    auto &graph = r.graph;

    auto external_images = r.gltf.images;

    graph.add_pass({
        .name = "glTF depth prepass",
        .type = PassType::Graphics,
        .external_images = external_images,
        .depth_attachment = r.depth_buffer,
        .exec = [pass_data=r.gltf](RenderGraph& /*graph*/, RenderPass &/*self*/, vulkan::API &api)
        {
            auto program = pass_data.prepass;

            api.bind_index_buffer(pass_data.index_buffer);
            api.bind_vertex_buffer(pass_data.vertex_buffer);

            draw_model(api, *pass_data.model, program);
        }
    });
}

static void add_gltf_pass(Renderer &r)
{
    auto &graph = r.graph;

    auto external_images = r.gltf.images;
    std::vector<ImageDescH> sampled_images;
    sampled_images.push_back(r.voxels_radiance);
    for (auto volume : r.voxels_directional_volumes) {
        sampled_images.push_back(volume);
    }
    for (auto cascade : r.shadow_cascades) {
        sampled_images.push_back(cascade);
    }

    auto &depth_slices_pos = r.depth_slices_pos;


    auto matrices_pos = r.matrices_pos;

    graph.add_pass({
        .name = "glTF pass",
        .type = PassType::Graphics,
        .external_images = external_images,
        .sampled_images = sampled_images,
        .color_attachments = {r.hdr_buffer},
        .depth_attachment = r.depth_buffer,
        .exec = [pass_data=r.gltf, voxel_data=r.voxels, trilinear_sampler=r.trilinear_sampler, depth_slices_pos, matrices_pos](RenderGraph& graph, RenderPass &self, vulkan::API &api)
        {
            auto voxels_radiance = graph.get_resolved_image(self.sampled_images[0]);
            std::vector<vulkan::ImageH> voxels_directional_volumes;
            std::vector<vulkan::ImageH> shadow_cascades;
            for (usize i = 1; i < 7; i++) {
                voxels_directional_volumes.push_back(graph.get_resolved_image(self.sampled_images[i]));
            }
            for (usize i = 7; i < self.sampled_images.size(); i++) {
                shadow_cascades.push_back(graph.get_resolved_image(self.sampled_images[i]));
            }

            auto program = pass_data.shading;

            api.bind_buffer(program, voxel_data.vct_debug_pos, vulkan::SHADER_DESCRIPTOR_SET, 1);
            api.bind_buffer(program, voxel_data.voxel_options_pos, vulkan::SHADER_DESCRIPTOR_SET, 2);

            api.bind_combined_image_sampler(program,
                                            api.get_image(voxels_radiance).default_view,
                                            trilinear_sampler,
                                            vulkan::SHADER_DESCRIPTOR_SET,
                                            3);

            {
                std::vector<vulkan::ImageViewH> views;
            views.reserve(voxels_directional_volumes.size());
            for (const auto &volume_h : voxels_directional_volumes) {
                views.push_back(api.get_image(volume_h).default_view);
            }
            api.bind_combined_images_samplers(program,
                                             views,
                                              {trilinear_sampler},
                                             vulkan::SHADER_DESCRIPTOR_SET,
                                             4);
            }

            api.bind_buffer(program, depth_slices_pos, vulkan::SHADER_DESCRIPTOR_SET, 5);
            api.bind_buffer(program, matrices_pos, vulkan::SHADER_DESCRIPTOR_SET, 6);

            {
                std::vector<vulkan::ImageViewH> views;
            views.reserve(shadow_cascades.size());
            for (const auto &cascade_h : shadow_cascades) {
                views.push_back(api.get_image(cascade_h).default_view);
            }
            api.bind_combined_images_samplers(program,
                                             views,
                                              {trilinear_sampler},
                                             vulkan::SHADER_DESCRIPTOR_SET,
                                             7);
            }

            api.bind_index_buffer(pass_data.index_buffer);
            api.bind_vertex_buffer(pass_data.vertex_buffer);

            draw_model(api, *pass_data.model, program);
        }
    });
}

/// --- Voxels

Renderer::VoxelPass create_voxel_pass(vulkan::API &api)
{
    Renderer::VoxelPass pass;

    {
        vulkan::GraphicsProgramInfo pinfo{};
        pinfo.vertex_shader   = api.create_shader("shaders/voxelization.vert.spv");
        pinfo.geom_shader     = api.create_shader("shaders/voxelization.geom.spv");
        pinfo.fragment_shader = api.create_shader("shaders/voxelization.frag.spv");

        pinfo.vertex_stride(sizeof(GltfVertex));
        pinfo.vertex_info({VK_FORMAT_R32G32B32_SFLOAT, MEMBER_OFFSET(GltfVertex, position)});
        pinfo.vertex_info({VK_FORMAT_R32G32B32_SFLOAT, MEMBER_OFFSET(GltfVertex, normal)});
        pinfo.vertex_info({VK_FORMAT_R32G32_SFLOAT, MEMBER_OFFSET(GltfVertex, uv0)});
        pinfo.vertex_info({VK_FORMAT_R32G32_SFLOAT, MEMBER_OFFSET(GltfVertex, uv1)});
        pinfo.vertex_info({VK_FORMAT_R32G32B32A32_SFLOAT, MEMBER_OFFSET(GltfVertex, joint0)});
        pinfo.vertex_info({VK_FORMAT_R32G32B32A32_SFLOAT, MEMBER_OFFSET(GltfVertex, weight0)});

        pass.voxelization = api.create_program(std::move(pinfo));
    }

    {
        vulkan::GraphicsProgramInfo pinfo{};
        pinfo.vertex_shader   = api.create_shader("shaders/voxel_points.vert.spv");
        pinfo.fragment_shader = api.create_shader("shaders/voxel_points.frag.spv");

        pinfo.topology = vulkan::PrimitiveTopology::PointList;

        pinfo.enable_depth_write = true;
        pinfo.depth_test         = VK_COMPARE_OP_GREATER_OR_EQUAL;

        pass.debug_visualization = api.create_program(std::move(pinfo));
    }

    pass.clear_voxels = api.create_program({
        .shader = api.create_shader("shaders/voxel_clear.comp.spv"),
    });

    pass.inject_radiance = api.create_program({
        .shader = api.create_shader("shaders/voxel_inject_direct_lighting.comp.spv"),
    });

    pass.generate_aniso_base = api.create_program({
        .shader = api.create_shader("shaders/voxel_gen_aniso_base.comp.spv"),
    });

    pass.generate_aniso_mipmap = api.create_program({
        .shader = api.create_shader("shaders/voxel_gen_aniso_mipmaps.comp.spv"),
    });

    return pass;
}

#if 1
static void add_voxels_clear_pass(Renderer& r)
{
    auto &graph = r.graph;
    auto &voxel_options = r.voxel_options;

    graph.add_pass({
        .name = "Voxels clear",
        .type = PassType::Compute,
        .storage_images = {r.voxels_albedo, r.voxels_normal, r.voxels_radiance},
        .exec = [pass_data=r.voxels, voxel_options](RenderGraph& graph, RenderPass &self, vulkan::API &api)
        {
            auto voxels_albedo = graph.get_resolved_image(self.storage_images[0]);
            auto voxels_normal = graph.get_resolved_image(self.storage_images[1]);
            auto voxels_radiance = graph.get_resolved_image(self.storage_images[2]);

            auto program = pass_data.clear_voxels;

            api.bind_buffer(program, pass_data.voxel_options_pos, 0);
            api.bind_image(program, api.get_image(voxels_albedo).default_view, 1);
            api.bind_image(program, api.get_image(voxels_normal).default_view, 2);
            api.bind_image(program, api.get_image(voxels_radiance).default_view, 3);

            auto count = voxel_options.res / 8;
            api.dispatch(program, count, count, count);
        }
    });
}
#else
static void add_voxels_clear_pass(Renderer& r)
{
    auto &graph = r.graph;

    graph.add_pass({
        .name = "Voxels clear",
        .type = PassType::Graphics,
        .color_attachments = {r.voxels_albedo, r.voxels_normal, r.voxels_radiance},
        .exec = [](RenderGraph& /*graph*/, RenderPass &/*self*/, vulkan::API &/*api*/) {}
    });

}
#endif

static void add_voxelization_pass(Renderer &r)
{
    auto &api   = r.api;
    auto &graph = r.graph;
    auto &ui    = *r.p_ui;

    if (ui.begin_window("Voxelization"))
    {
        ImGui::SliderFloat3("Center", &r.voxel_options.center.raw[0], -40.f, 40.f);
        ImGui::SliderFloat("Voxel size (m)", &r.voxel_options.size, 0.05f, 0.5f);
        ui.end_window();
    }

    auto &pass_data = r.voxels;

    // Upload voxel debug
    pass_data.voxel_options_pos = api.dynamic_uniform_buffer(sizeof(VoxelOptions));
    auto *buffer0                   = reinterpret_cast<VoxelOptions *>(pass_data.voxel_options_pos.mapped);
    *buffer0                        = r.voxel_options;


    // Upload projection cameras
    pass_data.projection_cameras     = api.dynamic_uniform_buffer(3 * sizeof(float4x4));
    auto *buffer1   = reinterpret_cast<float4x4 *>(pass_data.projection_cameras.mapped);
    float res      = r.voxel_options.res * r.voxel_options.size;
    float halfsize = res / 2;
    auto center = r.voxel_options.center + float3(halfsize);
    float3 min_clip = float3(-halfsize, -halfsize, 0.0f);
    float3 max_clip = float3( halfsize,  halfsize,  res);
    auto projection = Camera::ortho(min_clip, max_clip);
    buffer1[0] = projection * Camera::look_at(center + float3(halfsize, 0.f, 0.f), center, float3(0.f, 1.f, 0.f));
    buffer1[1] = projection * Camera::look_at(center + float3(0.f, halfsize, 0.f), center, float3(0.f, 0.f, -1.f));
    buffer1[2] = projection * Camera::look_at(center + float3(0.f, 0.f, halfsize), center, float3(0.f, 1.f, 0.f));


    graph.add_pass({
        .name = "Voxelization",
        .type = PassType::Graphics,
        .storage_images = {r.voxels_albedo, r.voxels_normal},
        .samples = VK_SAMPLE_COUNT_32_BIT,
        .exec = [pass_data, model_data=r.gltf, voxel_options=r.voxel_options](RenderGraph& graph, RenderPass &self, vulkan::API &api)
        {
            auto voxels_albedo = graph.get_resolved_image(self.storage_images[0]);
            auto voxels_normal = graph.get_resolved_image(self.storage_images[1]);

            auto program = pass_data.voxelization;

            api.set_viewport_and_scissor(voxel_options.res, voxel_options.res);

            api.bind_buffer(pass_data.voxelization, pass_data.voxel_options_pos, vulkan::SHADER_DESCRIPTOR_SET, 1);
            api.bind_buffer(pass_data.voxelization, pass_data.projection_cameras, vulkan::SHADER_DESCRIPTOR_SET, 2);

            auto &albedo_uint = api.get_image(voxels_albedo).format_views[0];
            auto &normal_uint = api.get_image(voxels_normal).format_views[0];
            api.bind_image(program, albedo_uint, vulkan::SHADER_DESCRIPTOR_SET, 3);
            api.bind_image(program, normal_uint, vulkan::SHADER_DESCRIPTOR_SET, 4);

            api.bind_index_buffer(model_data.index_buffer);
            api.bind_vertex_buffer(model_data.vertex_buffer);

            draw_model(api, *model_data.model, program);
        }
    });
}

static void add_voxels_debug_visualization_pass(Renderer& r)
{
    auto &graph = r.graph;
    auto &api = r.api;

    auto options = r.voxel_options;

    auto voxels = r.voxels_albedo;
    if (r.vct_debug.display_selected == 1) {
        voxels = r.voxels_normal;
    }
    else if (r.vct_debug.display_selected == 2) {
        voxels = r.voxels_radiance;
    }
    else if (r.vct_debug.display_selected == 3) {
        voxels = r.voxels_directional_volumes[0];
        options.size *= 2;
    }
    else if (r.vct_debug.display_selected == 4) {
        voxels = r.voxels_directional_volumes[1];
        options.size *= 2;
    }
    else if (r.vct_debug.display_selected == 5) {
        voxels = r.voxels_directional_volumes[2];
        options.size *= 2;
    }
    else if (r.vct_debug.display_selected == 6) {
        voxels = r.voxels_directional_volumes[3];
        options.size *= 2;
    }
    else if (r.vct_debug.display_selected == 7) {
        voxels = r.voxels_directional_volumes[4];
        options.size *= 2;
    }
    else if (r.vct_debug.display_selected == 8) {
        voxels = r.voxels_directional_volumes[5];
        options.size *= 2;
    }

    if (r.vct_debug.voxel_selected_mip > 0) {
        for (int i = 0; i < r.vct_debug.voxel_selected_mip; i++) {
            options.size *= 2;
        }
    }

    auto options_pos = api.dynamic_uniform_buffer(sizeof(VoxelOptions));
    auto *buffer0    = reinterpret_cast<VoxelOptions *>(options_pos.mapped);
    *buffer0         = options;

    auto vertex_count = r.voxel_options.res * r.voxel_options.res * r.voxel_options.res;

    graph.add_pass({
        .name = "Voxels debug visualization",
        .type = PassType::Graphics,
        .sampled_images = { voxels },
        .color_attachments = {r.hdr_buffer},
        .depth_attachment = r.depth_buffer,
        .exec = [pass_data=r.voxels, sampler=r.trilinear_sampler, options_pos, vertex_count](RenderGraph& graph, RenderPass &self, vulkan::API &api)
        {
            auto voxels = graph.get_resolved_image(self.sampled_images[0]);

            auto program = pass_data.debug_visualization;

            api.bind_buffer(program, options_pos, vulkan::SHADER_DESCRIPTOR_SET, 0);
            api.bind_buffer(program, pass_data.vct_debug_pos, vulkan::SHADER_DESCRIPTOR_SET, 1);

            api.bind_combined_image_sampler(program, api.get_image(voxels).default_view, sampler, vulkan::SHADER_DESCRIPTOR_SET, 2);

            api.bind_program(program);

            api.draw(vertex_count, 1, 0, 0);
        }
    });

}

static void add_voxels_direct_lighting_pass(Renderer &r)
{
    auto &graph = r.graph;

    const auto &depth_slices_pos = r.depth_slices_pos;
    const auto &matrices_pos = r.matrices_pos;

    std::vector<ImageDescH> sampled_images = {r.voxels_albedo, r.voxels_normal};
    for (auto cascade : r.shadow_cascades) {
        sampled_images.push_back(cascade);
    }

    graph.add_pass({
            .name = "Voxels direct lighting",
            .type = PassType::Compute,
            .sampled_images = sampled_images,
            .storage_images = {r.voxels_radiance},
            .exec =
            [pass_data=r.voxels, trilinear_sampler=r.trilinear_sampler, voxel_options=r.voxel_options, depth_slices_pos, matrices_pos]
            (RenderGraph &graph, RenderPass &self, vulkan::API &api)
            {
                auto voxels_albedo = graph.get_resolved_image(self.sampled_images[0]);
                auto voxels_normal = graph.get_resolved_image(self.sampled_images[1]);
                std::vector<vulkan::ImageH> shadow_cascades;
                shadow_cascades.reserve(self.sampled_images.size() - 2);
                for (usize i = 2; i < self.sampled_images.size(); i++) {
                    shadow_cascades.push_back(graph.get_resolved_image(self.sampled_images[i]));
                }

                auto voxels_radiance = graph.get_resolved_image(self.storage_images[0]);

                const auto &program = pass_data.inject_radiance;

                api.bind_buffer(program, pass_data.voxel_options_pos, 0);
                api.bind_buffer(program, pass_data.vct_debug_pos, 1);
                api.bind_combined_image_sampler(program, api.get_image(voxels_albedo).default_view, trilinear_sampler, 2);
                api.bind_combined_image_sampler(program, api.get_image(voxels_normal).default_view, trilinear_sampler, 3);
                api.bind_image(program, api.get_image(voxels_radiance).default_view, 4);

                api.bind_buffer(program, depth_slices_pos, 5);
                api.bind_buffer(program, matrices_pos, 6);

                {
                    std::vector<vulkan::ImageViewH> views;
                    views.reserve(shadow_cascades.size());
                    for (const auto &cascade_h : shadow_cascades) {
                        views.push_back(api.get_image(cascade_h).default_view);
                    }
                    api.bind_combined_images_samplers(program,
                                                     views,
                                                      {trilinear_sampler},
                                                     7);
                }


                auto count = voxel_options.res / 8;
                api.dispatch(program, count, count, count);
            }
        });
}

static void add_voxels_aniso_filtering(Renderer &r)
{
    auto &api   = r.api;
    auto &graph = r.graph;

    api.begin_label("Compute anisotropic voxels");

    auto voxel_size = r.voxel_options.size * 2;
    auto voxel_res  = r.voxel_options.res / 2;

    auto mip_pos = api.dynamic_uniform_buffer(sizeof(VoxelOptions));
    {
        auto *buffer = reinterpret_cast<VoxelOptions *>(mip_pos.mapped);
        *buffer      = r.voxel_options;
        buffer->size = voxel_size;
        buffer->res  = voxel_res;
    }

    std::vector<ImageDescH> storage_images;
    for (auto image : r.voxels_directional_volumes)
    {
        storage_images.push_back(image);
    }

    auto count = r.voxel_options.res / 8; // local compute size

    auto &trilinear_sampler = r.trilinear_sampler;

    graph.add_pass(
        {.name           = "Voxels aniso base",
         .type           = PassType::Compute,
         .sampled_images = {r.voxels_radiance},
         .storage_images = storage_images,
         .exec =
             [pass_data = r.voxels, trilinear_sampler, count, mip_pos](RenderGraph &graph,
                                                                                             RenderPass &self,
                                                                                             vulkan::API &api) {
                 // resolved directional volumes
                 std::vector<vulkan::ImageH> voxels_directional_volumes;
                 std::vector<vulkan::ImageViewH> views;
                 views.reserve(self.storage_images.size());
                 for (auto volume : self.storage_images)
                 {
                     auto volume_h = graph.get_resolved_image(volume);
                     voxels_directional_volumes.push_back(volume_h);

                     auto &image = api.get_image(volume_h);
                     views.push_back(image.mip_views[0]);
                 }

                 auto voxels_radiance = graph.get_resolved_image(self.sampled_images[0]);

                 auto program = pass_data.generate_aniso_base;

                 api.bind_buffer(program, mip_pos, 0);
                 api.bind_combined_image_sampler(program, api.get_image(voxels_radiance).default_view, trilinear_sampler, 1);
                 api.bind_images(program, views, 2);

                 api.dispatch(program, count, count, count);
             }

        });

    for (uint mip_i = 0; count > 1; mip_i++)
    {
        count /= 2;
        voxel_size *= 2;
        voxel_res /= 2;

        auto src = mip_i;
        auto dst = mip_i + 1;

        // Bind voxel options
        mip_pos = api.dynamic_uniform_buffer(sizeof(VoxelOptions));
        {
            auto *buffer = reinterpret_cast<VoxelOptions *>(mip_pos.mapped);
            *buffer      = r.voxel_options;
            buffer->size = voxel_size;
            buffer->res  = voxel_res;
        }

        auto mip_src_pos = api.dynamic_uniform_buffer(sizeof(int));
        {
            auto *buffer = reinterpret_cast<int *>(mip_src_pos.mapped);
            *buffer      = static_cast<int>(src);
        }

        graph.add_pass({.name           = "Voxels aniso mip level",
                        .type           = PassType::Compute,
                        .sampled_images = {r.voxels_radiance},
                        .storage_images = storage_images,
                        .exec =
                            [pass_data = r.voxels, count, mip_pos, mip_src_pos, src, dst](RenderGraph &graph,
                                                                                          RenderPass &self,
                                                                                          vulkan::API &api) {
                                // resolved directional volumes
                                std::vector<vulkan::ImageH> voxels_directional_volumes;
                                voxels_directional_volumes.reserve(self.storage_images.size());
                                for (auto volume : self.storage_images)
                                {
                                    auto volume_h = graph.get_resolved_image(volume);
                                    voxels_directional_volumes.push_back(volume_h);
                                }

                                auto program = pass_data.generate_aniso_mipmap;
                                api.bind_buffer(program, mip_pos, 0);
                                api.bind_buffer(program, mip_src_pos, 1);

                                std::vector<vulkan::ImageViewH> src_views;
                                std::vector<vulkan::ImageViewH> dst_views;
                                src_views.reserve(voxels_directional_volumes.size());
                                dst_views.reserve(voxels_directional_volumes.size());

                                for (const auto &volume_h : voxels_directional_volumes)
                                {
                                    auto &image = api.get_image(volume_h);
                                    src_views.push_back(image.mip_views[src]);
                                    dst_views.push_back(image.mip_views[dst]);
                                }

                                api.bind_images(program, src_views, 2);
                                api.bind_images(program, dst_views, 3);

                                api.dispatch(program, count, count, count);
                            }

        });
    }

    api.end_label();
}

/// ---

void update_uniforms(Renderer &r)
{
    auto &api = r.api;
    api.begin_label("Update uniforms");

    r.global_uniform_pos     = api.dynamic_uniform_buffer(sizeof(GlobalUniform));
    auto *globals            = reinterpret_cast<GlobalUniform *>(r.global_uniform_pos.mapped);
    std::memset(globals, 0, sizeof(GlobalUniform));

    globals->camera_pos      = r.p_camera->position;
    globals->camera_view     = r.p_camera->get_view();
    globals->camera_proj     = r.p_camera->get_projection();
    globals->camera_inv_proj = r.p_camera->get_inverse_projection();
    globals->camera_inv_view_proj = r.p_camera->get_inverse_view() * r.p_camera->get_inverse_projection();
    globals->camera_near     = r.p_camera->near_plane;
    globals->camera_far      = r.p_camera->far_plane;
    globals->sun_view        = r.sun.get_view();
    globals->sun_proj        = r.sun.get_projection();

    auto resolution_scale = r.settings.resolution_scale;
    globals->resolution      = {static_cast<uint>(resolution_scale * api.ctx.swapchain.extent.width), static_cast<uint>(resolution_scale * api.ctx.swapchain.extent.height)};
    globals->sun_direction   = -1.0f * r.sun.front;
    globals->sun_illuminance = r.sun_illuminance; // TODO: move from global and use real values (will need auto exposure)
    globals->ambient         = r.ambient;

    r.api.bind_buffer({}, r.global_uniform_pos, vulkan::GLOBAL_DESCRIPTOR_SET, 0);

    std::vector<vulkan::ImageViewH> views;
    std::vector<vulkan::SamplerH> samplers;

    for (uint i = 0; i < r.gltf.model->textures.size(); i++)
    {
        const auto& texture = r.gltf.model->textures[i];
        auto image_h = r.gltf.images[texture.image];
        auto &image = api.get_image(image_h);
        views.push_back(image.default_view);
        samplers.push_back(r.gltf.samplers[texture.sampler]);
    }

    api.bind_combined_images_samplers({}, views, samplers, vulkan::GLOBAL_DESCRIPTOR_SET, 1);

    r.api.update_global_set();

    r.voxels.vct_debug_pos = api.dynamic_uniform_buffer(sizeof(VCTDebug));
    auto *debug            = reinterpret_cast<VCTDebug *>(r.voxels.vct_debug_pos.mapped);
    *debug                 = r.vct_debug;


    r.voxels.voxel_options_pos = api.dynamic_uniform_buffer(sizeof(VoxelOptions));
    auto *buffer0                   = reinterpret_cast<VoxelOptions *>(r.voxels.voxel_options_pos.mapped);
    *buffer0                        = r.voxel_options;

    api.end_label();
}

/// --- Where the magic happens

void Renderer::display_ui(UI::Context &ui)
{
    graph.display_ui(ui);
    api.display_ui(ui);

    ImGuiIO &io  = ImGui::GetIO();
    auto &timer  = *p_timer;

    io.DeltaTime = timer.get_delta_time();
    io.Framerate = timer.get_average_fps();

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
            static std::array<float, 128> gpu_values;
            static std::array<float, 128> cpu_values;

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
            ImGui::PlotLines("", gpu_values.data(), 128, 0, "", 0.0f, 30000.0f, ImVec2(0, 80));

            ImGui::Text("%-17s: %7.1f ms", "Total CPU time", cpu_average);
            ImGui::PlotLines("", cpu_values.data(), 128, 0, "", 0.0f, 30000.0f, ImVec2(0, 80));
        }

        ui.end_window();
    }

    if (ui.begin_window("Settings"))
    {
        float scale = settings.resolution_scale;
        ImGui::SliderFloat("Resolution scale", &scale, 0.25f, 1.0f);
        (void)(scale);

        int cascades_count = settings.shadow_cascades_count;
        ImGui::InputInt("Cascades count", &cascades_count, 1, 2);
        (void)(cascades_count);
        p_ui->end_window();
    }

    if (ui.begin_window("Global"))
    {
        ImGui::SliderFloat("Sun Illuminance", &sun_illuminance.x, 1.0f, 100.0f);

        sun_illuminance.y = sun_illuminance.x;
        sun_illuminance.z = sun_illuminance.x;

        ImGui::SliderFloat3("Sun rotation", &sun.pitch, -180.0f, 180.0f);
        ImGui::SliderFloat("Ambient", &ambient, 0.0f, 1.0f);

        p_ui->end_window();
    }

    if (ui.begin_window("Voxel Cone Tracing", true))
    {
        ImGui::TextUnformatted("Debug display: ");
        ImGui::SameLine();
        if (ImGui::RadioButton("glTF", vct_debug.display == 0)) {
            vct_debug.display = 0;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("voxels", vct_debug.display == 1)) {
            vct_debug.display = 1;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("custom", vct_debug.display == 2)) {
            vct_debug.display = 2;
        }

        if (vct_debug.display == 0)
        {
            static std::array options{"Nothing", "BaseColor", "Normals", "AO", "Indirect lighting", "Direct lighting"};
            tools::imgui_select("Debug output", options.data(), options.size(), vct_debug.display_selected);
            ImGui::SliderFloat("Trace dist.", &vct_debug.trace_dist, 0.0f, 30.0f);
            ImGui::SliderFloat("Occlusion factor", &vct_debug.occlusion_lambda, 0.0f, 1.0f);
            ImGui::SliderFloat("Sampling factor", &vct_debug.sampling_factor, 0.1f, 2.0f);
            ImGui::SliderFloat("Start position (in voxel)", &vct_debug.start, 0.0f, 2.0f);
        }
        else
        {
            static std::array options{
                "Albedo",
                "Normal",
                "Radiance",
                "Voxels volume -X",
                "Voxels volume +X",
                "Voxels volume -Y",
                "Voxels volume +Y",
                "Voxels volume -Z",
                "Voxels volume +Z",
            };
            tools::imgui_select("Debug output", options.data(), options.size(), vct_debug.display_selected);

            ImGui::SliderInt("Mip level", &vct_debug.voxel_selected_mip, 0, 10);
        }

        ui.end_window();
    }

    if (ui.begin_window("Cascaded Shadow maps"))
    {
        ImGui::TextUnformatted("Depth slices:");
        for (auto depth_slice : depth_slices) {
            ImGui::Text("  %f", depth_slice);
        }
        for (auto shadow_map : shadow_cascades)
        {
            auto &res = graph.images.at(shadow_map);
            if (res.resolved_img.is_valid()) {
                ImGui::Image((void*)(api.get_image(res.resolved_img).default_view.hash()), ImVec2(512, 512));
            }
        }
        ui.end_window();
    }

    if (ui.begin_window("Camera", true))
    {
        ImGui::SliderFloat("Near plane", &p_camera->near_plane, 0.0f, 1.0f);
        ImGui::SliderFloat("Far plane", &p_camera->far_plane, 100.0f, 1000.0f);

        float aspect_ratio = api.ctx.swapchain.extent.width / float(api.ctx.swapchain.extent.height);
        p_camera->projection = Camera::perspective(60.0f, aspect_ratio, p_camera->near_plane, p_camera->far_plane, &p_camera->projection_inverse);
        ui.end_window();
    }

    sun.update_view();
}

void Renderer::draw()
{
    bool is_ok = api.start_frame();
    if (!is_ok) {
        ImGui::EndFrame();
        return;
    }
    graph.clear(); // start_frame() ?

    update_uniforms(*this);

    add_shadow_cascades_pass(*this);

    // voxel cone tracing prep
    add_voxels_clear_pass(*this);
    add_voxelization_pass(*this);
    add_voxels_direct_lighting_pass(*this);
    add_voxels_aniso_filtering(*this);

    if (vct_debug.display != 0)
    {
        add_voxels_debug_visualization_pass(*this);
    }
    else
    {
        add_gltf_prepass(*this);
        add_gltf_pass(*this);
    }

    add_procedural_sky_pass(*this);

    add_tonemapping_pass(*this);

    graph.add_pass({
        .name = "Blit to swapchain",
        .type = PassType::BlitToSwapchain,
        .color_attachments = {ldr_buffer}
    });

    add_floor_pass(*this);

    add_imgui_pass(*this);

    ImGui::EndFrame(); // right before drawing the ui

    if (!graph.execute()) {
        return;
    }

    api.end_frame();
}

} // namespace my_app
