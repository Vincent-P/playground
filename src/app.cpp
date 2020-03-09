#include "app.hpp"
#include <imgui.h>
#include <iostream>
#include "file_watcher.hpp"

namespace my_app
{

constexpr auto DEFAULT_WIDTH  = 1920;
constexpr auto DEFAULT_HEIGHT = 1080;

App::App() : window(DEFAULT_WIDTH, DEFAULT_HEIGHT)
{
    camera   = InputCamera::create(window, float3(0, 10, 0));
    renderer = Renderer::create(window, camera._internal);

    window.register_resize_callback([this](int width, int height) { this->renderer.on_resize(width, height); });
    window.register_mouse_callback([this](double xpos, double ypos) { this->camera.on_mouse_movement(xpos, ypos); });

    ImGuiIO &io  = ImGui::GetIO();
    io.DeltaTime = timer.get_delta_time();
    io.Framerate = timer.get_average_fps();

    io.DisplaySize.x             = float(renderer.api.ctx.swapchain.extent.width);
    io.DisplaySize.y             = float(renderer.api.ctx.swapchain.extent.height);
    io.DisplayFramebufferScale.x = window.get_dpi_scale().x;
    io.DisplayFramebufferScale.y = window.get_dpi_scale().y;

    watcher = FileWatcher::create();

    shaders_watch = watcher.add_watch("shaders");

    watcher.on_file_change([&](const auto &watch, const auto &event) {
        if (watch.wd != shaders_watch.wd) {
            return;
        }

        this->renderer.reload_shader("shaders", event);
    });
}

App::~App() { renderer.destroy(); }

void App::draw_fps()
{
    ImGuiIO &io  = ImGui::GetIO();
    io.DeltaTime = timer.get_delta_time();
    io.Framerate = timer.get_average_fps();

    io.DisplaySize.x             = float(renderer.api.ctx.swapchain.extent.width);
    io.DisplaySize.y             = float(renderer.api.ctx.swapchain.extent.height);
    io.DisplayFramebufferScale.x = window.get_dpi_scale().x;
    io.DisplayFramebufferScale.y = window.get_dpi_scale().y;

    static bool init = true;
    if (init) {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / window.get_dpi_scale().x - 120.0f, 10.0f * window.get_dpi_scale().y));
        ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
    }
    else {
        ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_NoScrollbar);
    }
    init = false;

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
        ImGui::PlotHistogram("", histogram.data(), static_cast<int>(histogram.size()), 0, nullptr, 0.0f, FLT_MAX, ImVec2(85.0f, 30.0f));
    }
    else {
        ImGui::SetCursorPosX(20.0f);
        ImGui::Text("%9.3f", double(timer.get_average_delta_time()));

        auto &histogram = timer.get_delta_time_histogram();
        ImGui::PlotHistogram("", histogram.data(), static_cast<int>(histogram.size()), 0, nullptr, 0.0f, FLT_MAX, ImVec2(85.0f, 30.0f));
    }

    ImGui::End();
}

void App::update()
{
    draw_fps();
    camera.update();
}

void App::run()
{
    while (!window.should_close()) {
        ImGui::NewFrame();
        window.update();
        update();
        timer.update();
        renderer.draw();
        watcher.update();
    }

    renderer.wait_idle();
}

} // namespace my_app
