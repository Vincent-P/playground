#include "app.hpp"
#if defined(ENABLE_IMGUI)
#include <imgui.h>
#endif
#include <iostream>
#include <sstream>
#include "file_watcher.hpp"

namespace my_app
{

constexpr auto DEFAULT_WIDTH  = 1920;
constexpr auto DEFAULT_HEIGHT = 1080;

App::App() : window(DEFAULT_WIDTH, DEFAULT_HEIGHT)
{
    camera   = InputCamera::create(window, timer, ui, float3(4.0f, 14.5f, 0.0f));
    camera._internal.yaw   = 90.0f;
    camera._internal.pitch = 0.0f;

    renderer = Renderer::create(window, camera._internal, timer, ui);

    window.register_resize_callback([this](int width, int height) { this->renderer.on_resize(width, height); });
    window.register_mouse_callback([this](double xpos, double ypos) { this->camera.on_mouse_movement(xpos, ypos); });
    window.register_scroll_callback([this](double xoffset, double yoffset) { this->camera.on_mouse_scroll(xoffset, yoffset); });

#if defined(ENABLE_IMGUI)
    ImGuiIO &io  = ImGui::GetIO();
    io.DeltaTime = timer.get_delta_time();
    io.Framerate = timer.get_average_fps();

    io.DisplaySize.x             = float(renderer.api.ctx.swapchain.extent.width);
    io.DisplaySize.y             = float(renderer.api.ctx.swapchain.extent.height);
    io.DisplayFramebufferScale.x = window.get_dpi_scale().x;
    io.DisplayFramebufferScale.y = window.get_dpi_scale().y;
#endif

    watcher = FileWatcher::create();

    shaders_watch = watcher.add_watch("shaders");

    watcher.on_file_change([&](const auto &watch, const auto &event) {
        if (watch.wd != shaders_watch.wd) {
            return;
        }


        std::stringstream shader_name_stream;
        shader_name_stream << "shaders/" << event.name;
        std::string shader_name = shader_name_stream.str();

        this->renderer.reload_shader(shader_name);
    });
}

App::~App() { renderer.destroy(); }

void App::update()
{
    camera.update();
}

void App::run()
{
    while (!window.should_close()) {
#if defined(ENABLE_IMGUI)
        ImGui::NewFrame();
        ui.display();
#endif
        window.update();
        update();
        timer.update();
        renderer.draw();
        watcher.update();
    }

    renderer.wait_idle();
}

} // namespace my_app
