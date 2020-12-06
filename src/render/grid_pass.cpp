#include "render/renderer.hpp"

namespace my_app
{

/// --- Checker board floor

Renderer::CheckerBoardFloorPass create_floor_pass(vulkan::API &api)
{
    Renderer::CheckerBoardFloorPass pass;

    vulkan::DepthState depth = {
        .test = VK_COMPARE_OP_GREATER_OR_EQUAL,
    };
    pass.program = api.create_program({
        .vertex_shader   = api.create_shader("shaders/checkerboard_floor.vert.spv"),
        .fragment_shader = api.create_shader("shaders/checkerboard_floor.frag.spv"),
        .depth           = depth,
    });

    return pass;
}

void add_floor_pass(Renderer &r)
{
    auto &graph = r.graph;

    graph.add_pass({
        .name              = "Checkerboard Floor pass",
        .type              = PassType::Graphics,
        .color_attachments = {r.ldr_buffer},
        .depth_attachment  = r.depth_buffer,
        .exec =
            [pass_data = r.checkerboard_floor](RenderGraph & /*graph*/, RenderPass & /*self*/, vulkan::API &api) {
                auto program = pass_data.program;
                api.bind_program(program);
                api.draw(6, 1, 0, 0);
            },
    });
}
} // namespace my_app
