#include "app.hpp"
#include <variant>
#include <imgui/imgui.h>
#include <iostream>
#include <sstream>
#include "file_watcher.hpp"

namespace my_app
{

constexpr auto DEFAULT_WIDTH  = 1920;
constexpr auto DEFAULT_HEIGHT = 1080;

App::App()
{
    window::Window::create(window, DEFAULT_WIDTH, DEFAULT_HEIGHT, "Test vulkan");
    UI::Context::create(ui);

    InputCamera::create(camera, window, timer, float3(4.0f, 14.5f, 0.0f));
    camera._internal.yaw   = 90.0f;
    camera._internal.pitch = 0.0f;

    Renderer::create(renderer, window, camera._internal, timer, ui);

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

    is_minimized = false;
}

App::~App()
{
    ui.destroy();
    renderer.destroy();
}

void App::update()
{
    camera.update();
}

void App::display_ui()
{
    ui.start_frame(window);
    ui.display_ui();
    camera.display_ui(ui);
    renderer.display_ui(ui);
}

void App::run()
{
    while (!window.should_close()) {
        window.poll_events();

        std::optional<window::event::Resize> last_resize;
        for (auto &event : window.events) {
            if (std::holds_alternative<window::event::Resize>(event))
            {
                auto resize = std::get<window::event::Resize>(event);
                last_resize = resize;
            }
            else if (std::holds_alternative<window::event::Scroll>(event))
            {
                auto scroll = std::get<window::event::Scroll>(event);
                this->camera.on_mouse_scroll(double(scroll.dx), double(scroll.dy));
            }
            else if (std::holds_alternative<window::event::MouseMove>(event))
            {
                auto move = std::get<window::event::MouseMove>(event);
                this->camera.on_mouse_movement(double(move.x), double(move.y));
                this->is_minimized = false;
            }
            else if (std::holds_alternative<window::event::Key>(event))
            {
                auto key = std::get<window::event::Key>(event);
                if (key.key == window::VirtualKey::Escape) {
                    window.stop = true;
                }
            }
        }

        if (last_resize)
        {
            auto resize = *last_resize;
            if (resize.width > 0 && resize.height > 0) {
                renderer.on_resize(resize.width, resize.height);
            }
            if (window.minimized) {
                this->is_minimized = true;
            }
        }

        window.events.clear();

        if (is_minimized) {
            continue;
        }

        update();
        timer.update();
        display_ui();
        renderer.draw();
        watcher.update();

    }

    renderer.wait_idle();
}

} // namespace my_app
