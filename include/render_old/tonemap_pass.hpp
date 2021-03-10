#pragma once

#include "render/hl_api.hpp"
#include "render/render_graph.hpp"

namespace my_app
{

struct LuminancePass;

struct TonemappingPass
{
    vulkan::ComputeProgramH program;
    vulkan::CircularBufferPosition params_pos;

    struct Debug
    {
        uint selected = 0;
        float exposure = 1.0f;
    } debug;
};

TonemappingPass create_tonemapping_pass(vulkan::API &api);
void add_tonemapping_pass(RenderGraph &graph, TonemappingPass &pass_data, const LuminancePass &luminance_pass, ImageDescH input, ImageDescH output);

} // namespace my_app
