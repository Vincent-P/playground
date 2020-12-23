#include "render/hl_api.hpp"
#include "render/renderer.hpp"

namespace my_app
{

    Renderer::TonemappingPass create_tonemapping_pass(vulkan::API &api)
    {
        Renderer::TonemappingPass pass;

        pass.program = api.create_program({
                .shader   = api.create_shader("shaders/tonemap.comp.glsl.spv"),
            });

        pass.params_pos = {};

        return pass;
    }

    void add_tonemapping_pass(Renderer &r)
    {
        auto &api   = r.api;
        auto &graph = r.graph;

        // Make a shader debugging window and its own uniform buffer
        {
            r.tonemapping.params_pos = api.dynamic_uniform_buffer(sizeof(uint) + sizeof(float));
            auto *buffer             = reinterpret_cast<uint *>(r.tonemapping.params_pos.mapped);
            buffer[0]                = r.tonemap_debug.selected;
            auto *floatbuffer        = reinterpret_cast<float *>(buffer + 1);
            floatbuffer[0]           = r.tonemap_debug.exposure;
        }

        auto output = r.override_main_pass_output ? *r.override_main_pass_output : r.hdr_buffer;

        graph.add_pass({
                .name              = "Tonemapping",
                .type              = PassType::Compute,
                .sampled_images    = {output, r.average_luminance},
                .storage_images    = {r.ldr_buffer},
                .exec =
                [pass_data       = r.tonemapping,
                 default_sampler = r.nearest_sampler](RenderGraph &graph, RenderPass &self, vulkan::API &api) {
                    auto hdr_buffer = graph.get_resolved_image(self.sampled_images[0]);
                    auto average_luminance       = graph.get_resolved_image(self.sampled_images[1]);
                    auto output       = graph.get_resolved_image(self.storage_images[0]);
                    auto output_image = api.get_image(output);
                    auto program    = pass_data.program;

                    api.bind_combined_image_sampler(program,
                                                    hdr_buffer,
                                                    default_sampler,
                                                    0);

                    api.bind_buffer(program, pass_data.params_pos, 1);

                    api.bind_combined_image_sampler(program,
                                                    average_luminance,
                                                    default_sampler,
                                                    2);

                    api.bind_image(program, output, 3);



                    auto size_x = static_cast<uint>(output_image.info.width / 16) + 1;
                    auto size_y = static_cast<uint>(output_image.info.height / 16) + 1;
                    api.dispatch(program, size_x, size_y, 1);
                },
            });
    }

} // namespace my_app
