#include "render/hl_api.hpp"
#include "render/renderer.hpp"
#include <vulkan/vulkan_core.h>

namespace my_app
{
Renderer::LuminancePass create_luminance_pass(vulkan::API &api)
{
    (void)(api);
    Renderer::LuminancePass pass;
    pass.build_histo = api.create_program({
        .shader = api.create_shader("shaders/build_luminance_histo.comp.spv"),
    });

    pass.average_histo = api.create_program({
        .shader = api.create_shader("shaders/average_luminance_histo.comp.spv"),
    });

    pass.histogram_buffer = api.create_buffer({
        .name  = "Luminance histogram",
        .size  = 256 * sizeof(float) * 4 /*debug*/,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    });
    return pass;
}

void add_luminance_pass(Renderer &r)
{
    auto &graph = r.graph;

    graph.add_pass({
        .name           = "Build histogram",
        .type           = PassType::Compute,
        .sampled_images = {r.hdr_buffer},
        .exec =
            [pass_data         = r.luminance,
             trilinear_sampler = r.trilinear_sampler](RenderGraph &graph, RenderPass &self, vulkan::API &api) {

                auto program    = pass_data.build_histo;
                auto hdr_buffer = graph.get_resolved_image(self.sampled_images[0]);
                auto hdr_buffer_image = api.get_image(hdr_buffer);

                api.bind_combined_image_sampler(program,
                                                hdr_buffer_image.default_view,
                                                trilinear_sampler,
                                                0);

                api.bind_buffer(program, pass_data.histogram_buffer, 1);

                struct UBO {
                    uint input_width;
                    uint input_height;
                    float min_log_luminance;
                    float one_over_log_luminance_range;
                };

                auto uniform = api.dynamic_uniform_buffer(sizeof(UBO));
                auto *u = reinterpret_cast<UBO *>(uniform.mapped);
                u->input_width = hdr_buffer_image.info.width;
                u->input_height = hdr_buffer_image.info.height;
                u->min_log_luminance = -10.f;
                u->one_over_log_luminance_range = 1.f / 12.f;

                api.bind_buffer(program, uniform, 2);

                api.clear_buffer(pass_data.histogram_buffer, 0u);

                auto size_x = static_cast<uint>(hdr_buffer_image.info.width / 16);
                auto size_y = static_cast<uint>(hdr_buffer_image.info.height / 16);
                api.dispatch(program, size_x, size_y, 1);
            },
    });

    uint pixel_count = r.settings.resolution_scale * (r.settings.render_resolution.x * r.settings.render_resolution.y);

    graph.add_pass({
        .name           = "Average histogram",
        .type           = PassType::Compute,
        .storage_images = {r.average_luminance},
        .exec =
        [pass_data = r.luminance, pixel_count](RenderGraph &graph, RenderPass &self, vulkan::API &api) {
                auto program                 = pass_data.average_histo;
                auto average_luminance       = graph.get_resolved_image(self.storage_images[0]);
                auto average_luminance_image = api.get_image(average_luminance);

                api.bind_image(program, average_luminance_image.default_view, 0);

                api.bind_buffer(program, pass_data.histogram_buffer, 1);

                struct UBO
                {
                    uint  pixel_count;
                    float min_log_luminance;
                    float log_luminance_range;
                    float tau;
                };

                auto uniform           = api.dynamic_uniform_buffer(sizeof(UBO));
                auto *u                = reinterpret_cast<UBO *>(uniform.mapped);
                u->pixel_count         = pixel_count;
                u->min_log_luminance   = -10.f;
                u->log_luminance_range = 12.f;
                u->tau                 = 1.1f;

                api.bind_buffer(program, uniform, 2);
                api.dispatch(program, 1, 1, 1);
            },
    });
}
} // namespace my_app
