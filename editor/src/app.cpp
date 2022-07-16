#include "app.h"

#include <exo/os/file_watcher.h>
#include <exo/os/platform.h>
#include <exo/os/window.h>
#include <exo/memory/scope_stack.h>

#include <render/renderer.h>
#include <render/render_world_system.h>
#include <engine/assets/asset_manager.h>
#include <engine/ui.h>

#include <Tracy.hpp>

constexpr auto DEFAULT_WIDTH  = 1920;
constexpr auto DEFAULT_HEIGHT = 1080;

App *App::create(exo::ScopeStack &scope)
{
    auto *app = scope.allocate<App>();

	app->platform = reinterpret_cast<exo::Platform*>(scope.allocate(exo::platform_get_size()));
	exo::platform_create(app->platform);

    app->window        = exo::Window::create(app->platform, scope, {DEFAULT_WIDTH, DEFAULT_HEIGHT}, "Editor");
    app->asset_manager = AssetManager::create(scope);
    app->asset_manager->load_all_metas();

    app->inputs.bind(Action::QuitApp, {.keys = {exo::VirtualKey::Escape}});
    app->inputs.bind(Action::CameraModifier, {.keys = {exo::VirtualKey::LAlt}});
    app->inputs.bind(Action::CameraMove, {.mouse_buttons = {exo::MouseButton::Left}});
    app->inputs.bind(Action::CameraOrbit, {.mouse_buttons = {exo::MouseButton::Right}});

    UI::create_context(app->window, &app->inputs);

    app->renderer = Renderer::create(scope, app->window, app->asset_manager);

    UI::new_frame();

    app->watcher       = exo::FileWatcher::create();
    app->shaders_watch = app->watcher.add_watch("shaders");
    app->watcher.on_file_change([=](const auto &watch, const auto &event) {
        if (watch.wd != app->shaders_watch.wd)
        {
            return;
        }

        std::string shader_name = fmt::format("shaders/{}", event.name);
        app->renderer->reload_shader(shader_name);
    });
    app->asset_manager->setup_file_watcher(app->watcher);

    app->is_minimized = false;

    app->scene.init(app->asset_manager, &app->inputs);

    return app;
}

App::~App()
{
    scene.destroy();
    UI::destroy_context();
	exo::platform_destroy(platform);
}

void App::display_ui()
{
    ZoneScoped;

    UI::display_ui();

    renderer->display_ui();
    inputs.display_ui();
    scene.display_ui();
    asset_manager->display_ui();
}

void App::run()
{
    while (!window->should_close())
    {
        window->poll_events();

        Option<exo::events::Resize> last_resize;
        for (auto &event : window->events)
        {
            if (event.type == exo::Event::ResizeType)
            {
                last_resize = event.resize;
            }
            else if (event.type == exo::Event::MouseMoveType)
            {
                const auto &move = event.mouse_move;
                // this->ui.on_mouse_movement(*window, double(move.x), double(move.y));

                this->is_minimized = false;
            }
        }

        inputs.process(window->events);

        if (inputs.is_pressed(Action::QuitApp))
        {
            window->stop = true;
        }

        if (last_resize)
        {
            auto resize = *last_resize;
            if (resize.width > 0 && resize.height > 0)
            {
                renderer->on_resize();
            }
            if (window->minimized)
            {
                this->is_minimized = true;
            }
        }

        window->events.clear();
        if (window->should_close()) {
            break;
        }

        if (!is_minimized)
        {
            display_ui();
            scene.update(inputs);
            render_world = scene.entity_world.get_system_registry().get_system<PrepareRenderWorld>()->render_world;
            renderer->update(render_world);
        }

        watcher.update();
        FrameMark
    }
}
