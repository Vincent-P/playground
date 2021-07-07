#include "render/hl_api.hpp"
#include "render/render_graph.hpp"
#include "render/renderer.hpp"

#include "render/luminance_pass.hpp"

namespace my_app
{

TonemappingPass create_tonemapping_pass(vulkan::API &api)
{
    TonemappingPass pass;

    pass.program = api.create_program({
        .shader = api.create_shader("shaders/tonemap.comp.glsl.spv"),
    });

    pass.params_pos = {};

    return pass;
}

void add_tonemapping_pass(RenderGraph &graph, TonemappingPass &pass_data, const LuminancePass &luminance_pass, ImageDescH input, ImageDescH output)
{
    auto &api = *graph.p_api;

    // Make a shader debugging window and its own uniform buffer
    {
        pass_data.params_pos = api.dynamic_uniform_buffer(sizeof(uint) + sizeof(float));
        auto *buffer             = reinterpret_cast<uint *>(pass_data.params_pos.mapped);
        buffer[0]                = pass_data.debug.selected;
        auto *floatbuffer        = reinterpret_cast<float *>(buffer + 1);
        floatbuffer[0]           = pass_data.debug.exposure;
    }

    graph.add_pass({
        .name           = "Tonemapping",
        .type           = PassType::Compute,
        .sampled_images = {input, luminance_pass.average_luminance},
        .storage_images = {output},
        .exec =
            [=](RenderGraph &graph, RenderPass &self, vulkan::API &api) {
                auto hdr_buffer        = graph.get_resolved_image(self.sampled_images[0]);
                auto average_luminance = graph.get_resolved_image(self.sampled_images[1]);
                auto output            = graph.get_resolved_image(self.storage_images[0]);
                auto program           = pass_data.program;

                api.bind_combined_image_sampler(program, hdr_buffer, api.nearest_sampler, 0);
                api.bind_buffer(program, pass_data.params_pos, 1);
                api.bind_combined_image_sampler(program, average_luminance, api.nearest_sampler, 2);
                api.bind_image(program, output, 3);
                api.dispatch(program, api.dispatch_size(output, 16));
            },
    });
}

} // namespace my_app
