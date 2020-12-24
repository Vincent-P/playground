#include "render/sky_pass.hpp"

#include "components/sky_atmosphere_component.hpp"

#define assert_uniform_size(T) \
    static_assert(sizeof(T) % 16 == 0, "Uniforms must be aligned to a float4!"); \
    static_assert(sizeof(T) < 64_KiB, "Uniforms maximum range is 64KiB")

namespace my_app
{

ProceduralSkyPass create_procedural_sky_pass(vulkan::API &api)
{
    ProceduralSkyPass pass;

    vulkan::RasterizationState rasterization = {.culling = false};

    pass.render_transmittance = api.create_program({
        .vertex_shader   = api.create_shader("shaders/fullscreen_triangle.vert.spv"),
        .fragment_shader = api.create_shader("shaders/transmittance_lut.frag.spv"),
        .rasterization   = rasterization,
    });

    pass.render_skyview = api.create_program({
        .vertex_shader   = api.create_shader("shaders/fullscreen_triangle.vert.spv"),
        .fragment_shader = api.create_shader("shaders/skyview_lut.frag.spv"),
        .rasterization   = rasterization,
    });

    pass.sky_raymarch = api.create_program({
        .vertex_shader   = api.create_shader("shaders/fullscreen_triangle.vert.spv"),
        .fragment_shader = api.create_shader("shaders/sky_raymarch.frag.spv"),
        .rasterization   = rasterization,
    });

    pass.compute_multiscattering_lut = api.create_program({
        .shader = api.create_shader("shaders/multiscat_lut.comp.spv"),
    });

    return pass;
}

void add_procedural_sky_pass(RenderGraph &graph, ProceduralSkyPass &pass_data, const SkyAtmosphereComponent &sky_atmosphere, ImageDescH depth_buffer, ImageDescH output)
{
    assert_uniform_size(AtmosphereParameters);
    static_assert(sizeof(AtmosphereParameters) == 240);

    auto &api = *graph.p_api;

    pass_data.atmosphere_params_pos = api.dynamic_uniform_buffer(sizeof(AtmosphereParameters));
    auto *p = reinterpret_cast<AtmosphereParameters *>(pass_data.atmosphere_params_pos.mapped);
    *p      = parameters_from_component(sky_atmosphere);

    pass_data.transmittance_lut = graph.image_descs.add({
        .name      = "Transmittance LUT",
        .size_type = SizeType::Absolute,
        .size      = float3(256, 64, 1),
        .format    = VK_FORMAT_R16G16B16A16_SFLOAT,
    });

    pass_data.skyview_lut = graph.image_descs.add({
        .name      = "Skyview LUT",
        .size_type = SizeType::Absolute,
        .size      = float3(192, 108, 1),
        .format    = VK_FORMAT_R16G16B16A16_SFLOAT,
    });

    pass_data.multiscattering_lut = graph.image_descs.add({
        .name      = "Multiscattering LUT",
        .size_type = SizeType::Absolute,
        .size      = float3(32, 32, 1),
        .format    = VK_FORMAT_R16G16B16A16_SFLOAT,
    });

    graph.add_pass({
        .name              = "Transmittance LUT",
        .type              = PassType::Graphics,
        .color_attachments = {pass_data.transmittance_lut},
        .exec =
            [=](RenderGraph & /*graph*/, RenderPass & /*self*/, vulkan::API &api) {
                auto program = pass_data.render_transmittance;
                api.bind_buffer(program, pass_data.atmosphere_params_pos, vulkan::SHADER_DESCRIPTOR_SET, 0);
                api.bind_program(program);
                api.draw(3, 1, 0, 0);
            },
    });

    graph.add_pass({
        .name           = "Sky Multiscattering LUT",
        .type           = PassType::Compute,
        .sampled_images = {pass_data.transmittance_lut},
        .storage_images = {pass_data.multiscattering_lut},
        .exec =
            [=](RenderGraph &graph, RenderPass &self, vulkan::API &api) {
                auto transmittance   = graph.get_resolved_image(self.sampled_images[0]);
                auto multiscattering = graph.get_resolved_image(self.storage_images[0]);
                auto program         = pass_data.compute_multiscattering_lut;

                api.bind_buffer(program, pass_data.atmosphere_params_pos, 0);
                api.bind_combined_image_sampler(program, transmittance, api.trilinear_sampler, 1);
                api.bind_image(program, multiscattering, 2);
                api.dispatch(program, api.dispatch_size(multiscattering, 1));
            },
    });

    graph.add_pass({
        .name              = "Skyview LUT",
        .type              = PassType::Graphics,
        .sampled_images    = {pass_data.transmittance_lut, pass_data.multiscattering_lut},
        .color_attachments = {pass_data.skyview_lut},
        .exec =
            [=](RenderGraph &graph, RenderPass &self, vulkan::API &api) {
                auto transmittance   = graph.get_resolved_image(self.sampled_images[0]);
                auto multiscattering = graph.get_resolved_image(self.sampled_images[1]);
                auto program         = pass_data.render_skyview;

                api.bind_buffer(program, pass_data.atmosphere_params_pos, vulkan::SHADER_DESCRIPTOR_SET, 0);

                api.bind_combined_image_sampler(program,
                                                transmittance,
                                                api.trilinear_sampler,
                                                vulkan::SHADER_DESCRIPTOR_SET,
                                                1);

                api.bind_combined_image_sampler(program,
                                                multiscattering,
                                                api.trilinear_sampler,
                                                vulkan::SHADER_DESCRIPTOR_SET,
                                                2);

                api.bind_program(program);

                api.draw(3, 1, 0, 0);
            },
    });

    graph.add_pass({
        .name              = "Sky raymarch",
        .type              = PassType::Graphics,
        .sampled_images    = {pass_data.transmittance_lut, pass_data.multiscattering_lut, depth_buffer, pass_data.skyview_lut},
        .color_attachments = {output},
        .exec =
            [=](RenderGraph &graph, RenderPass &self, vulkan::API &api) {
                auto transmittance   = graph.get_resolved_image(self.sampled_images[0]);
                auto multiscattering = graph.get_resolved_image(self.sampled_images[1]);
                auto depth           = graph.get_resolved_image(self.sampled_images[2]);
                auto skyview         = graph.get_resolved_image(self.sampled_images[3]);
                auto program         = pass_data.sky_raymarch;

                api.bind_buffer(program, pass_data.atmosphere_params_pos, vulkan::SHADER_DESCRIPTOR_SET, 0);

                api.bind_combined_image_sampler(program,
                                                transmittance,
                                                api.trilinear_sampler,
                                                vulkan::SHADER_DESCRIPTOR_SET,
                                                1);

                api.bind_combined_image_sampler(program, skyview, api.trilinear_sampler, vulkan::SHADER_DESCRIPTOR_SET, 2);

                api.bind_combined_image_sampler(program, depth, api.nearest_sampler, vulkan::SHADER_DESCRIPTOR_SET, 3);

                api.bind_combined_image_sampler(program,
                                                multiscattering,
                                                api.trilinear_sampler,
                                                vulkan::SHADER_DESCRIPTOR_SET,
                                                4);

                api.bind_program(program);

                api.draw(3, 1, 0, 0);
            },
    });
}

} // namespace my_app
