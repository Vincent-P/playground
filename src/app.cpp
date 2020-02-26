#include "app.hpp"
#include <imgui.h>

namespace my_app
{

constexpr auto DEFAULT_WIDTH  = 1280;
constexpr auto DEFAULT_HEIGHT = 720;

App::App() : window(DEFAULT_WIDTH, DEFAULT_HEIGHT)
{
    renderer = Renderer::create(window);
    window.register_resize_callback([this](int width, int height) { this->renderer.on_resize(width, height); });
}

App::~App() { renderer.destroy(); }

void App::draw_fps()
{
    ImGuiIO &io  = ImGui::GetIO();
    io.DeltaTime = timer.get_delta_time();
    io.Framerate = timer.get_average_fps();

    io.DisplaySize.x = float(renderer.api.ctx.swapchain.extent.width);
    io.DisplaySize.y = float(renderer.api.ctx.swapchain.extent.height);

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 120.0f, 20.0f));
    ImGui::SetNextWindowSize(ImVec2(100.0f, 100.0));
    ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);

    static bool show_fps = false;

    if (ImGui::RadioButton("FPS", show_fps)) {
        show_fps = true;
    }

    ImGui::SameLine();

    if (ImGui::RadioButton("ms", !show_fps)) {
        show_fps = false;
    }

    if (show_fps) {
        ImGui::SetCursorPosX(20.0f);
        ImGui::Text("%7.1f", double(timer.get_average_fps()));

        auto &histogram = timer.get_fps_histogram();
        ImGui::PlotHistogram("", histogram.data(), static_cast<int>(histogram.size()), 0, nullptr, 0.0f, FLT_MAX,
                             ImVec2(85.0f, 30.0f));
    }
    else {
        ImGui::SetCursorPosX(20.0f);
        ImGui::Text("%9.3f", double(timer.get_average_delta_time()));

        auto &histogram = timer.get_delta_time_histogram();
        ImGui::PlotHistogram("", histogram.data(), static_cast<int>(histogram.size()), 0, nullptr, 0.0f, FLT_MAX,
                             ImVec2(85.0f, 30.0f));
    }

    ImGui::End();
}

void App::run()
{
    while (!window.should_close()) {
        ImGui::NewFrame();
        window.update();
        draw_fps();
        timer.update();
        renderer.draw();
    }

    renderer.wait_idle();
}

} // namespace my_app
