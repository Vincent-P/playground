#pragma once
#include "renderer/vlk_context.hpp"
#include "renderer/hl_api.hpp"

/***
 * The renderer is the orchestrator of the Vulkan Context and the HL API.
 * The main functions are StartFrame and EndFrame and it contains
 * raw HL API calls in between to draw things or validate/cook a Render Graph.
 ***/

namespace my_app
{
    struct Renderer
    {
	vulkan::API api;

	static Renderer create(const Window& window);
	void destroy();

	void on_resize(int width, int height);
	void wait_idle();

	void imgui_draw();
	void draw();

	vulkan::ProgramH gui_program;
	vulkan::ImageH gui_texture;

	vulkan::RenderTargetH rt;
    };
}
