#include "render/renderer.hpp"
#include "../shaders/include/atmosphere.h"

namespace my_app
{

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

void add_procedural_sky_pass(Renderer &r)
{
    auto &api   = r.api;
    auto &graph = r.graph;

    assert_uniform_size(AtmosphereParameters);
    static_assert(sizeof(AtmosphereParameters) == 240);

    r.procedural_sky.atmosphere_params_pos = api.dynamic_uniform_buffer(sizeof(AtmosphereParameters));
    auto *p = reinterpret_cast<AtmosphereParameters *>(r.procedural_sky.atmosphere_params_pos.mapped);

    {
        // info.solar_irradiance = { 1.474000f, 1.850400f, 1.911980f };

        // Using a normalise sun illuminance. This is to make sure the LUTs acts as a
        // transfert factor to apply the runtime computed sun irradiance over.
        p->solar_irradiance   = {1.0f, 1.0f, 1.0f};
        p->sun_angular_radius = 0.004675f;

        // Earth
        p->bottom_radius = 6360000.0f;
        p->top_radius    = 6460000.0f;
        p->ground_albedo = {0.0f, 0.0f, 0.0f};

        // Raleigh scattering
        constexpr double kRayleighScaleHeight = 8000.0;
        constexpr double kMieScaleHeight      = 1200.0;

        p->rayleigh_density.width     = 0.0f;
        p->rayleigh_density.layers[0] = DensityProfileLayer{
            .exp_term  = 1.0f,
            .exp_scale = -1.0f / kRayleighScaleHeight,
        };
        p->rayleigh_scattering = {0.000005802f, 0.000013558f, 0.000033100f};

        // Mie scattering
        p->mie_density.width     = 0.0f;
        p->mie_density.layers[0] = DensityProfileLayer{.exp_term = 1.0f, .exp_scale = -1.0f / kMieScaleHeight};

        p->mie_scattering       = {0.000003996f, 0.000003996f, 0.000003996f};
        p->mie_extinction       = {0.000004440f, 0.000004440f, 0.000004440f};
        p->mie_phase_function_g = 0.8f;

        // Ozone absorption
        p->absorption_density.width     = 25000.0f;
        p->absorption_density.layers[0] = DensityProfileLayer{
            .linear_term   = 1.0f / 15000.0f,
            .constant_term = -2.0f / 3.0f,
        };

        p->absorption_density.layers[1] = DensityProfileLayer{
            .linear_term   = -1.0f / 15000.0f,
            .constant_term = 8.0f / 3.0f,
        };
        p->absorption_extinction = {0.000000650f, 0.000001881f, 0.000000085f};

        const double max_sun_zenith_angle = PI * 120.0 / 180.0; // (use_half_precision_ ? 102.0 : 120.0) / 180.0 * kPi;
        p->mu_s_min                       = (float)cos(max_sun_zenith_angle);
    }

    graph.add_pass({
        .name              = "Transmittance LUT",
        .type              = PassType::Graphics,
        .color_attachments = {r.transmittance_lut},
        .exec =
            [pass_data = r.procedural_sky](RenderGraph & /*graph*/, RenderPass & /*self*/, vulkan::API &api) {
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
                api.bind_combined_image_sampler(program,
                                                api.get_image(transmittance).default_view,
                                                trilinear_sampler,
                                                1);
                api.bind_image(program, api.get_image(multiscattering).default_view, 2);

                auto multiscattering_desc = *graph.image_descs.get(self.storage_images[0]);
                auto size_x               = static_cast<uint>(multiscattering_desc.size.x);
                auto size_y               = static_cast<uint>(multiscattering_desc.size.y);
                api.dispatch(program, size_x, size_y, 1);
            },
    });

    graph.add_pass({
        .name              = "Skyview LUT",
        .type              = PassType::Graphics,
        .sampled_images    = {r.transmittance_lut, r.multiscattering_lut},
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
        .name              = "Sky raymarch",
        .type              = PassType::Graphics,
        .sampled_images    = {r.transmittance_lut, r.multiscattering_lut, r.depth_buffer, r.skyview_lut},
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

                api.bind_combined_image_sampler(program,
                                                api.get_image(skyview).default_view,
                                                trilinear_sampler,
                                                vulkan::SHADER_DESCRIPTOR_SET,
                                                2);

                api.bind_combined_image_sampler(program,
                                                api.get_image(depth).default_view,
                                                nearest_sampler,
                                                vulkan::SHADER_DESCRIPTOR_SET,
                                                3);

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

} // namespace my_app
