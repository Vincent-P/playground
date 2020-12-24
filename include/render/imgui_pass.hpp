#pragma once

#include "render/hl_api.hpp"
#include "render/render_graph.hpp"

namespace my_app
{

struct ImGuiPass
{
    vulkan::GraphicsProgramH float_program;
    vulkan::GraphicsProgramH uint_program;
    vulkan::ImageH font_atlas;
};

ImGuiPass create_imgui_pass(vulkan::API &api);
void add_imgui_pass(RenderGraph &graph, ImGuiPass &pass_data, ImageDescH output);

};
