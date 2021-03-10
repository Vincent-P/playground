#include "render/luminance_pass.hpp"

#include "render/hl_api.hpp"
#include "render/render_graph.hpp"

#include <vulkan/vulkan_core.h>

namespace my_app
{

LuminancePass create_luminance_pass(vulkan::API &api)
{
    LuminancePass pass{};

    pass.build_histo = api.create_program({
        .shader = api.create_shader("shaders/build_luminance_histo.comp.spv"),
    });

    pass.average_histo = api.create_program({
        .shader = api.create_shader("shaders/average_luminance_histo.comp.spv"),
    });

    pass.histogram_buffer = api.create_buffer({
        .name  = "Luminance histogram",
        .size  = 256 * sizeof(float),
        .usage = vulkan::storage_buffer_usage,
    });

    return pass;
}

void add_luminance_pass(RenderGraph &graph, LuminancePass &pass_data, ImageDescH input)
{
    pass_data.average_luminance = graph.image_descs.add({
        .name          = "Average luminance",
        .size_type     = SizeType::Absolute,
        .size          = float3(1.0f),
        .type          = VK_IMAGE_TYPE_2D,
        .format        = VK_FORMAT_R32_SFLOAT,
    });

    graph.add_pass({
        .name           = "Build histogram",
        .type           = PassType::Compute,
        .sampled_images = {input},
        .exec =
            [=](RenderGraph &graph, RenderPass &self, vulkan::API &api) {
                struct UBO
                {
                    uint input_width;
                    uint input_height;
                    float min_log_luminance;
                    float one_over_log_luminance_range;
                };

                auto hdr_buffer       = graph.get_resolved_image(self.sampled_images[0]);
                auto hdr_buffer_image = api.get_image(hdr_buffer);

                auto uniform                    = api.dynamic_uniform_buffer(sizeof(UBO));
                auto *u                         = reinterpret_cast<UBO *>(uniform.mapped);
                u->input_width                  = hdr_buffer_image.info.width;
                u->input_height                 = hdr_buffer_image.info.height;
                u->min_log_luminance            = -10.f;
                u->one_over_log_luminance_range = 1.f / 12.f;

                auto program = pass_data.build_histo;

                api.clear_buffer(pass_data.histogram_buffer, 0u);

                api.bind_combined_image_sampler(program, hdr_buffer, api.trilinear_sampler, 0);
                api.bind_buffer(program, pass_data.histogram_buffer, 1);
                api.bind_buffer(program, uniform, 2);
                api.dispatch(program, api.dispatch_size(hdr_buffer, 16));
            },
    });

    graph.add_pass({
        .name           = "Average histogram",
        .type           = PassType::Compute,
        .sampled_images = {input},
        .storage_images = {pass_data.average_luminance},
        .exec =
            [=](RenderGraph &graph, RenderPass &self, vulkan::API &api) {
                auto hdr_buffer        = graph.get_resolved_image(self.sampled_images[0]);
                auto average_luminance = graph.get_resolved_image(self.storage_images[0]);
                auto hdr_buffer_image  = api.get_image(hdr_buffer);

                struct UBO
                {
                    uint pixel_count;
                    float min_log_luminance;
                    float log_luminance_range;
                    float tau;
                };

                auto uniform           = api.dynamic_uniform_buffer(sizeof(UBO));
                auto *u                = reinterpret_cast<UBO *>(uniform.mapped);
                u->pixel_count         = hdr_buffer_image.info.width * hdr_buffer_image.info.height;
                u->min_log_luminance   = -10.f;
                u->log_luminance_range = 12.f;
                u->tau                 = 1.1f;

                auto program           = pass_data.average_histo;
                api.bind_image(program, average_luminance, 0);
                api.bind_buffer(program, pass_data.histogram_buffer, 1);
                api.bind_buffer(program, uniform, 2);
                api.dispatch(program, {1, 1, 1});
            },
    });
}

} // namespace my_app
