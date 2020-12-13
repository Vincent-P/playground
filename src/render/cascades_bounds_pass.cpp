#include "render/hl_api.hpp"
#include "render/renderer.hpp"
#include <vulkan/vulkan_core.h>

namespace my_app
{
Renderer::CascadesBoundsPass create_cascades_bounds_pass(vulkan::API &api)
{
    (void)(api);
    Renderer::CascadesBoundsPass pass;
    pass.depth_reduction_0 = api.create_program({
        .shader = api.create_shader("shaders/depth_reduction.comp.glsl.spv"),
    });

    pass.depth_reduction_1 = api.create_program({
        .shader = api.create_shader("shaders/depth_reduction_iter.comp.glsl.spv"),
    });
    return pass;
}

void add_cascades_bounds_pass(Renderer &r)
{
    auto &graph = r.graph;

    float2 resolution = float2(r.settings.resolution_scale * r.settings.render_resolution.x,
                               r.settings.resolution_scale * r.settings.render_resolution.y);

    r.depth_reduction_maps.clear();

    for (; resolution.x != 1.0f && resolution.y != 1.0f;)
    {
        resolution.x = std::ceil(resolution.x / 16.0f);
        resolution.y = std::ceil(resolution.y / 16.0f);

        r.depth_reduction_maps.push_back(graph.image_descs.add({
            .name      = "Depth reduction",
            .size_type = SizeType::Absolute,
            .size      = float3(resolution.x, resolution.y, 1.0),
            .type      = VK_IMAGE_TYPE_2D,
            .format    = VK_FORMAT_R32G32_SFLOAT,
        }));
    }

    graph.add_pass({
        .name           = "Reduce Depth first",
        .type           = PassType::Compute,
        .sampled_images = {r.depth_buffer},
        .storage_images = {r.depth_reduction_maps[0]},
        .exec =
            [pass_data         = r.cascades_bounds,
             trilinear_sampler = r.trilinear_sampler](RenderGraph &graph, RenderPass &self, vulkan::API &api) {
                auto depth_buffer        = graph.get_resolved_image(self.sampled_images[0]);
                auto depth_buffer_image  = api.get_image(depth_buffer);
                auto reduction_map       = graph.get_resolved_image(self.storage_images[0]);
                auto reduction_map_image = api.get_image(reduction_map);

                auto program = pass_data.depth_reduction_0;

                api.bind_combined_image_sampler(program, depth_buffer_image.default_view, trilinear_sampler, 0);
                api.bind_image(program, reduction_map_image.default_view, 1);

                api.dispatch(program, reduction_map_image.info.width, reduction_map_image.info.height, 1);
            },
    });

    for (usize i_reduction = 0; i_reduction < r.depth_reduction_maps.size() - 1; i_reduction++)
    {
        graph.add_pass({
            .name           = "Reduce Depth final",
            .type           = PassType::Compute,
            .sampled_images = {r.depth_reduction_maps[i_reduction]},
            .storage_images = {r.depth_reduction_maps[i_reduction + 1]},
            .exec =
            [pass_data = r.cascades_bounds, trilinear_sampler=r.trilinear_sampler](RenderGraph &graph, RenderPass &self, vulkan::API &api) {
                    auto input        = graph.get_resolved_image(self.sampled_images[0]);
                    auto input_image  = api.get_image(input);
                    auto output       = graph.get_resolved_image(self.storage_images[0]);
                    auto output_image = api.get_image(output);

                    auto program = pass_data.depth_reduction_1;

                    api.bind_combined_image_sampler(program, input_image.default_view, trilinear_sampler, 0);
                    api.bind_image(program, output_image.default_view, 1);

                    api.dispatch(program, output_image.info.width, output_image.info.height, 1);
                },
        });
    }
}
} // namespace my_app
