#pragma once

#include "render/hl_api.hpp"
#include "render/render_graph.hpp"

namespace my_app
{

struct LuminancePass
{
    vulkan::BufferH histogram_buffer;
    vulkan::ComputeProgramH build_histo;
    vulkan::ComputeProgramH average_histo;
    ImageDescH average_luminance;
};

LuminancePass create_luminance_pass(vulkan::API &api);
void add_luminance_pass(RenderGraph &graph, LuminancePass &pass_data, ImageDescH input);

} // namespace my_app
