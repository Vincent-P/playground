#include "app.h"

#include <engine/scene.h>
#include <exo/memory/scope_stack.h>

#include <cross/file_watcher.h>
#include <cross/platform.h>
#include <cross/window.h>

#include <assets/asset_id_formatter.h>
#include <assets/asset_manager.h>
#include <assets/subscene.h>
#include <engine/render_world_system.h>
#include <painter/painter.h>

#include <Tracy.hpp>

#include <fmt/format.h>
#include <string_view>
#include <ui/docking.h>
#include <ui/scroll.h>
#include <ui/ui.h>

#define SOKOL_TIME_IMPL
#include <sokol_time.h>

constexpr auto DEFAULT_WIDTH  = 1920;
constexpr auto DEFAULT_HEIGHT = 1080;

App *App::create(exo::ScopeStack &scope)
{
	auto *app = scope.allocate<App>();

	auto *platform = reinterpret_cast<cross::platform::Platform *>(scope.allocate(cross::platform::get_size()));
	cross::platform::create(platform);

	app->window        = cross::Window::create(scope, {DEFAULT_WIDTH, DEFAULT_HEIGHT}, "Editor");
	app->asset_manager = AssetManager::create(scope);

	app->inputs.bind(Action::QuitApp, {.keys = {exo::VirtualKey::Escape}});
	app->inputs.bind(Action::CameraModifier, {.keys = {exo::VirtualKey::LAlt}});
	app->inputs.bind(Action::CameraMove, {.mouse_buttons = {exo::MouseButton::Left}});
	app->inputs.bind(Action::CameraOrbit, {.mouse_buttons = {exo::MouseButton::Right}});

	app->watcher  = cross::FileWatcher::create();
	app->renderer = Renderer::create(app->window->get_win32_hwnd(), app->asset_manager);

	int   font_size_pt = 24;
	float font_size_px = float(font_size_pt);

	app->ui_font                      = Font::from_file(ASSET_PATH "/SpaceGrotesk.otf", font_size_pt);
	app->painter                      = painter_allocate(scope, 8_MiB, 8_MiB, int2(1024, 1024));
	app->painter->glyph_atlas_gpu_idx = app->renderer.glyph_atlas_index();

	app->ui      = ui::create(&app->ui_font, font_size_px, app->painter);
	app->docking = docking::create();

	app->is_minimized = false;

	app->scene.init(app->asset_manager, &app->inputs);

#if 0
	auto  scene_id    = AssetId::create<SubScene>("NewSponza_Main_Blender_glTF.gltf");
	auto *scene_asset = app->asset_manager->load_asset<SubScene>(scene_id);
	app->scene.import_subscene(scene_asset);
#endif

	stm_setup();

	return app;
}

App::~App()
{
	scene.destroy();
	cross::platform::destroy();
}

void App::display_ui(double dt)
{
	this->ui.painter->index_offset        = 0;
	this->ui.painter->vertex_bytes_offset = 0;
	ui::new_frame(this->ui);

	auto fullscreen_rect = Rect{.pos = {0, 0}, .size = float2(int2(this->window->size.x, this->window->size.y))};
	auto em              = this->ui.theme.font_size;

	docking::begin_docking(this->docking, this->ui, fullscreen_rect);

	if (auto view_rect = docking::tabview(this->ui, this->docking, "View 1"); view_rect) {
		ui::label_in_rect(ui, view_rect.value(), "test");
	}

	if (auto view_rect = docking::tabview(this->ui, this->docking, "View 2"); view_rect) {
		ui::label_in_rect(ui, view_rect.value(), "test 2");
	}

	if (auto view_rect = docking::tabview(this->ui, this->docking, "Docking"); view_rect) {
		auto content_rect = rect_inset(view_rect.value(), float2(1.0f * em));
		ui::push_clip_rect(this->ui, ui::register_clip_rect(this->ui, view_rect.value()));
		docking::inspector_ui(this->docking, this->ui, content_rect);
		ui::pop_clip_rect(this->ui);
	}

	if (auto view_rect = docking::tabview(this->ui, this->docking, "Scene"); view_rect) {
		auto content_rect = rect_inset(view_rect.value(), float2(1.0f * em));
		ui::push_clip_rect(this->ui, ui::register_clip_rect(this->ui, content_rect));
		scene_debug_ui(this->ui, this->scene, content_rect);
		ui::pop_clip_rect(this->ui);
	}

	if (auto view_rect = docking::tabview(this->ui, this->docking, "Inputs"); view_rect) {
		auto content_rect = rect_inset(view_rect.value(), float2(1.0f * em));
		ui::push_clip_rect(this->ui, ui::register_clip_rect(this->ui, content_rect));

		auto rectsplit = RectSplit{content_rect, SplitDirection::Top};
		ui::label_split(this->ui, rectsplit, "Mouse buttons pressed:");
		for (auto pressed : this->inputs.mouse_buttons_pressed) {
			ui::label_split(this->ui, rectsplit, fmt::format("  {}", pressed));
		}

		ui::pop_clip_rect(this->ui);
	}

	if (auto view_rect = docking::tabview(this->ui, this->docking, "Asset Manager"); view_rect) {
		auto content_rect = rect_inset(view_rect.value(), float2(1.0f * em));
		ui::push_clip_rect(this->ui, ui::register_clip_rect(this->ui, content_rect));

		static auto scroll_offset = float2();
		auto rectsplit = RectSplit{content_rect, SplitDirection::Top};

		// margin
		rectsplit.split(10.0f * em);
		const auto &cliprect = ui::current_clip_rect(this->ui);
		ui::label_split(this->ui,
			rectsplit,
			fmt::format("clip rect {{pos: {}x{}, size: {}x{}}} ",
				cliprect.pos.x,
				cliprect.pos.y,
				cliprect.size.x,
				cliprect.size.y));

		ui::label_split(this->ui, rectsplit, fmt::format("Resources (offset {}):", scroll_offset.y));

		auto scrollarea_rect   = rectsplit.split(20.0f * em);
		auto inner_content_rect = ui::begin_scroll_area(this->ui, scrollarea_rect, scroll_offset);

		auto scroll_rectsplit   = RectSplit{inner_content_rect, SplitDirection::Top};
		for (auto [handle, p_resource] : this->asset_manager->database.resource_records) {
			ui::label_split(this->ui,
				scroll_rectsplit,
				fmt::format("  - {} {}", p_resource->asset_id, p_resource->resource_path.view()));
		}

		ui::end_scroll_area(this->ui, inner_content_rect);

		ui::pop_clip_rect(this->ui);
	}

	docking::end_docking(this->docking, this->ui);

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

	ui::end_frame(this->ui);
	this->window->set_cursor(static_cast<cross::Cursor>(this->ui.state.cursor));
}

void App::run()
{
	u64 last = stm_now();

	while (!window->should_close()) {
		window->poll_events();

		for (auto &event : window->events) {
			if (event.type == exo::Event::MouseMoveType) {
				this->is_minimized = false;
			}
		}

		inputs.process(window->events);
		inputs.main_window_size                          = window->size;
		this->ui.inputs.mouse_position                   = inputs.mouse_position;
		this->ui.inputs.mouse_buttons_pressed_last_frame = this->ui.inputs.mouse_buttons_pressed;
		this->ui.inputs.mouse_buttons_pressed            = this->inputs.mouse_buttons_pressed;

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

		watcher.update([&](const cross::Watch & /*watch*/, const cross::WatchEvent & /*event*/) {
			// this->asset_manager->on_file_change(watch, event);
		});

		FrameMark
	}
}
