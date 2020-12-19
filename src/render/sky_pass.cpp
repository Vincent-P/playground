#include "render/renderer.hpp"
#include "components/sky_atmosphere_component.hpp"

namespace my_app
{

Renderer::ProceduralSkyPass create_procedural_sky_pass(vulkan::API &api)
{
    Renderer::ProceduralSkyPass pass;

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

void add_procedural_sky_pass(Renderer &r, const SkyAtmosphereComponent& sky_atmosphere)
{
    auto &api   = r.api;
    auto &graph = r.graph;

    assert_uniform_size(AtmosphereParameters);
    static_assert(sizeof(AtmosphereParameters) == 240);

    r.procedural_sky.atmosphere_params_pos = api.dynamic_uniform_buffer(sizeof(AtmosphereParameters));
    auto *p = reinterpret_cast<AtmosphereParameters *>(r.procedural_sky.atmosphere_params_pos.mapped);
    *p = parameters_from_component(sky_atmosphere);

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
                                                transmittance,
                                                trilinear_sampler,
                                                1);
                api.bind_image(program, multiscattering, 2);

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
                                                transmittance,
                                                trilinear_sampler,
                                                vulkan::SHADER_DESCRIPTOR_SET,
                                                1);

                api.bind_combined_image_sampler(program,
                                                multiscattering,
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
                                                transmittance,
                                                trilinear_sampler,
                                                vulkan::SHADER_DESCRIPTOR_SET,
                                                1);

                api.bind_combined_image_sampler(program,
                                                skyview,
                                                trilinear_sampler,
                                                vulkan::SHADER_DESCRIPTOR_SET,
                                                2);

                api.bind_combined_image_sampler(program,
                                                depth,
                                                nearest_sampler,
                                                vulkan::SHADER_DESCRIPTOR_SET,
                                                3);

                api.bind_combined_image_sampler(program,
                                                multiscattering,
                                                trilinear_sampler,
                                                vulkan::SHADER_DESCRIPTOR_SET,
                                                4);

                api.bind_program(program);

                api.draw(3, 1, 0, 0);
            },
    });
}

} // namespace my_app
