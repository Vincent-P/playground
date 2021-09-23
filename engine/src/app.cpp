#include "app.h"

#include <exo/logger.h>
#include "camera.h"
#include <cross/file_watcher.h>

#include <algorithm>
#include <imgui/imgui.h>
#include <variant>
#include <tracy/Tracy.hpp>
#include <cstdlib>

constexpr auto DEFAULT_WIDTH  = 1920;
constexpr auto DEFAULT_HEIGHT = 1080;

void *operator new(std::size_t count)
{
    auto ptr = malloc(count);
    TracyAlloc(ptr, count);
    return ptr;
}
void operator delete(void *ptr) noexcept
{
    TracyFree(ptr);
    free(ptr);
}


App::App()
{
    platform::Window::create(window, DEFAULT_WIDTH, DEFAULT_HEIGHT, "Test vulkan");
    ui = UI::Context::create();

    renderer = Renderer::create(window, &asset_manager);

    watcher       = platform::FileWatcher::create();
    shaders_watch = watcher.add_watch("shaders");
    watcher.on_file_change([&](const auto &watch, const auto &event) {
        if (watch.wd != shaders_watch.wd)
        {
            return;
        }

        std::string shader_name = fmt::format("shaders/{}", event.name);
        this->renderer.reload_shader(shader_name);
    });

    is_minimized = false;

    inputs.bind(Action::QuitApp, {.keys = {VirtualKey::Escape}});
    inputs.bind(Action::CameraModifier, {.keys = {VirtualKey::LAlt}});
    inputs.bind(Action::CameraMove, {.mouse_buttons = {MouseButton::Left}});
    inputs.bind(Action::CameraOrbit, {.mouse_buttons = {MouseButton::Right}});

    scene.init(&asset_manager);
}

App::~App()
{
    scene.destroy();
    ui.destroy();
    renderer.destroy();
    window.destroy();
}


void App::display_ui()
{
    ui.start_frame(window, inputs);

    ui.display_ui();
    renderer.display_ui(ui);
    inputs.display_ui(ui);
    scene.display_ui(ui);
    // asset_manager.display_ui(ui);
}

void App::run()
{
    while (!window.should_close())
    {
        window.poll_events();

        Option<platform::event::Resize> last_resize;
        for (auto &event : window.events)
        {
            if (std::holds_alternative<platform::event::Resize>(event))
            {
                auto resize = std::get<platform::event::Resize>(event);
                last_resize = resize;
            }
            else if (std::holds_alternative<platform::event::MouseMove>(event))
            {
                auto move = std::get<platform::event::MouseMove>(event);
                this->ui.on_mouse_movement(window, double(move.x), double(move.y));

                this->is_minimized = false;
            }
        }

        inputs.process(window.events);

        if (inputs.is_pressed(Action::QuitApp))
        {
            window.stop = true;
        }

        if (last_resize)
        {
            auto resize = *last_resize;
            if (resize.width > 0 && resize.height > 0)
            {
                renderer.on_resize();
            }
            if (window.minimized)
            {
                this->is_minimized = true;
            }
        }

        window.events.clear();

        if (is_minimized)
        {
            continue;
        }

        display_ui();
        scene.update(inputs);
        renderer.update(scene);
        watcher.update();
        FrameMark
    }
}
