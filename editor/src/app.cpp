#include "app.h"

#include <cross/file_watcher.h>
#include <cross/platform.h>
#include <cross/window.h>
#include <exo/memory/scope_stack.h>

#include <assets/asset_manager.h>
#include <assets/subscene.h>

#include <engine/render_world_system.h>

#include <Tracy.hpp>

#include <string_view>

constexpr auto DEFAULT_WIDTH  = 1920;
constexpr auto DEFAULT_HEIGHT = 1080;

App *App::create(exo::ScopeStack &scope)
{
	auto *app = scope.allocate<App>();

	app->platform = reinterpret_cast<cross::Platform *>(scope.allocate(cross::platform_get_size()));
	cross::platform_create(app->platform);

	app->window        = cross::Window::create(app->platform, scope, {DEFAULT_WIDTH, DEFAULT_HEIGHT}, "Editor");
	app->asset_manager = AssetManager::create(scope);
	app->asset_manager->load_all_metas();

	app->inputs.bind(Action::QuitApp, {.keys = {exo::VirtualKey::Escape}});
	app->inputs.bind(Action::CameraModifier, {.keys = {exo::VirtualKey::LAlt}});
	app->inputs.bind(Action::CameraMove, {.mouse_buttons = {exo::MouseButton::Left}});
	app->inputs.bind(Action::CameraOrbit, {.mouse_buttons = {exo::MouseButton::Right}});

	app->watcher = cross::FileWatcher::create();
	app->asset_manager->setup_file_watcher(app->watcher);
	app->renderer = Renderer::create(app->window->get_win32_hwnd());

	app->is_minimized = false;

	app->scene.init(app->asset_manager, &app->inputs);

	using namespace std::literals;
	auto scene_uuid = exo::UUID::from_string("82f1c7fe-4b5b7fcc-23ca8a83-6ac8704a"sv);
	auto result     = app->asset_manager->load_asset(scene_uuid);
	ASSERT(result);
	SubScene *scene_asset = dynamic_cast<SubScene *>(result.value());
	ASSERT(scene_asset);
	app->scene.import_subscene(scene_asset);

	return app;
}

App::~App()
{
	scene.destroy();
	cross::platform_destroy(platform);
}

void App::run()
{
	while (!window->should_close()) {
		window->poll_events();

		for (auto &event : window->events) {
			if (event.type == exo::Event::MouseMoveType) {
				const auto &move   = event.mouse_move;
				this->is_minimized = false;
			}
		}

		inputs.process(window->events);

		if (inputs.is_pressed(Action::QuitApp)) {
			window->stop = true;
		}

		if (window->minimized) {
			this->is_minimized = true;
		}

		window->events.clear();
		if (window->should_close()) {
			break;
		}

		if (!is_minimized) {
			scene.update(inputs);
			render_world = scene.entity_world.get_system_registry().get_system<PrepareRenderWorld>()->render_world;
			renderer.draw(render_world);
		}

		watcher.update();
		FrameMark
	}
}
