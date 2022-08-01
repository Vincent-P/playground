#include "app.h"

#include <exo/memory/scope_stack.h>

#include <cross/file_watcher.h>
#include <cross/platform.h>
#include <cross/window.h>

#include <assets/asset_manager.h>
#include <assets/subscene.h>
#include <engine/render_world_system.h>
#include <painter/painter.h>

#include <Tracy.hpp>

#include <string_view>

#define SOKOL_TIME_IMPL
#include <sokol_time.h>

constexpr auto DEFAULT_WIDTH  = 1920;
constexpr auto DEFAULT_HEIGHT = 1080;

App *App::create(exo::ScopeStack &scope)
{
	auto *app = scope.allocate<App>();

	app->platform = reinterpret_cast<cross::Platform *>(scope.allocate(cross::platform_get_size()));
	cross::platform_create(app->platform);

	app->window        = cross::Window::create(app->platform, scope, {DEFAULT_WIDTH, DEFAULT_HEIGHT}, "Editor");
	app->asset_manager = AssetManager::create(scope);

	app->inputs.bind(Action::QuitApp, {.keys = {exo::VirtualKey::Escape}});
	app->inputs.bind(Action::CameraModifier, {.keys = {exo::VirtualKey::LAlt}});
	app->inputs.bind(Action::CameraMove, {.mouse_buttons = {exo::MouseButton::Left}});
	app->inputs.bind(Action::CameraOrbit, {.mouse_buttons = {exo::MouseButton::Right}});

	app->watcher  = cross::FileWatcher::create();
	app->renderer = Renderer::create(app->window->get_win32_hwnd(), app->asset_manager);

	app->ui.ui_font                      = Font::from_file(R"(C:\Windows\Fonts\segoeui.ttf)", 13);
	app->ui.painter                      = painter_allocate(scope, 8_MiB, 8_MiB, int2(1024, 1024));
	app->ui.painter->glyph_atlas_gpu_idx = app->renderer.glyph_atlas_index();
	app->ui.ui_state.painter             = app->ui.painter;
	app->ui.ui_theme.main_font           = &app->ui.ui_font;

	app->is_minimized = false;

	app->scene.init(app->asset_manager, &app->inputs);

	auto  scene_id    = AssetId::create<SubScene>("NewSponza_Main_Blender_glTF.gltf");
	auto *scene_asset = app->asset_manager->load_asset<SubScene>(scene_id);
	app->scene.import_subscene(scene_asset);

	stm_setup();

	return app;
}

App::~App()
{
	scene.destroy();
	cross::platform_destroy(platform);
}

void App::display_ui(double dt)
{
	this->ui.painter->index_offset        = 0;
	this->ui.painter->vertex_bytes_offset = 0;
	ui_new_frame(this->ui.ui_state);

	auto fullscreen_rect = Rect{.pos = {0, 0}, .size = float2(int2(this->window->size.x, this->window->size.y))};

	float em = 15.0f;

	histogram.push_time(float(dt));
	auto histogram_rect = Rect{
		.pos =
			{
				fullscreen_rect.pos[0] + fullscreen_rect.size[0] - 250.0f - 1.0f * em,
				1.0f * em,
			},
		.size = {250.0f, 150.0f},
	};

	custom_ui::histogram(this->ui,
		custom_ui::FpsHistogramWidget{
			.rect      = histogram_rect,
			.histogram = &this->histogram,
		});

	ui_end_frame(this->ui.ui_state);
	this->window->set_cursor(static_cast<cross::Cursor>(this->ui.ui_state.cursor));
}

void App::run()
{
	u64 last = stm_now();

	while (!window->should_close()) {
		window->poll_events();

		for (auto &event : window->events) {
			if (event.type == exo::Event::MouseMoveType) {
				const auto &move   = event.mouse_move;
				this->is_minimized = false;
			}
		}

		inputs.process(window->events);
		inputs.main_window_size = window->size;

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
			u64    now  = stm_now();
			u64    diff = stm_diff(now, last);
			double dt   = stm_sec(diff);

			last = now;
			this->display_ui(dt);
			this->scene.update(inputs);
			this->render_world =
				this->scene.entity_world.get_system_registry().get_system<PrepareRenderWorld>()->render_world;
			this->renderer.draw(render_world, this->ui.painter);
		}

		watcher.update([&](const cross::Watch &watch, const cross::WatchEvent &event) {
			// this->asset_manager->on_file_change(watch, event);
		});

		FrameMark
	}
}
