#pragma once
#include "renderer/hl_api.hpp"
#include "renderer/vlk_context.hpp"
#include "gltf.hpp"

/***
 * The renderer is the orchestrator of the Vulkan Context and the HL API.
 * The main functions are StartFrame and EndFrame and it contains
 * raw HL API calls in between to draw things or validate/cook a Render Graph.
 ***/

namespace my_app
{
struct Model;

struct Renderer
{
    vulkan::API api;

    static Renderer create(const Window &window);
    void destroy();

    void draw();
    void on_resize(int width, int height);
    void wait_idle();

    // glTF
    void load_model_data();
    void draw_model();

    // ImGui
    void imgui_draw();

    // ImGui
    vulkan::ProgramH gui_program;
    vulkan::ImageH gui_texture;
    // glTF
    Model model;

    vulkan::RenderTargetH rt;
    const Window *p_window;
};

} // namespace my_app
