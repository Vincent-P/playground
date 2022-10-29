#include "app.h"

#include <engine/camera.h>
#include <engine/scene.h>
#include <exo/memory/scope_stack.h>
#include <exo/profile.h>

#include <cross/file_watcher.h>
#include <cross/platform.h>
#include <cross/window.h>

#include <assets/asset_id_formatter.h>
#include <assets/asset_manager.h>
#include <assets/subscene.h>
#include <engine/render_world_system.h>
#include <painter/painter.h>

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
	EXO_PROFILE_SCOPE;

	auto *app = scope.allocate<App>();

	auto *platform = reinterpret_cast<cross::platform::Platform *>(scope.allocate(cross::platform::get_size()));
	cross::platform::create(platform);

	app->jobmanager = cross::JobManager::create();

	app->window        = cross::Window::create(scope, {DEFAULT_WIDTH, DEFAULT_HEIGHT}, "Editor");
	app->asset_manager = AssetManager::create(scope, app->jobmanager);

	app->inputs.bind(Action::QuitApp, {.keys = {exo::VirtualKey::Escape}});
	app->inputs.bind(Action::CameraModifier, {.keys = {exo::VirtualKey::LAlt}});
	app->inputs.bind(Action::CameraMove, {.mouse_buttons = {exo::MouseButton::Left}});
	app->inputs.bind(Action::CameraOrbit, {.mouse_buttons = {exo::MouseButton::Right}});

	app->watcher  = cross::FileWatcher::create();
	app->renderer = Renderer::create(app->window->get_win32_hwnd(), app->asset_manager);

	const int  font_size_pt = 18;
	const auto font_size_px = float(font_size_pt);

	app->ui_font                      = Font::from_file(ASSET_PATH "/SpaceGrotesk.otf", font_size_pt);
	app->painter                      = painter_allocate(scope, 1_MiB, 1_MiB, int2(1024, 1024));
	app->painter->glyph_atlas_gpu_idx = 0; // null texture

	app->ui      = ui::create(&app->ui_font, font_size_px, app->painter);
	app->docking = docking::create();

	app->is_minimized = false;

	app->scene.init(app->asset_manager, &app->inputs);

#if 0
	auto  scene_id    = AssetId::create<SubScene>("NewSponza_Main_Blender_glTF.gltf");
	auto *scene_asset = app->asset_manager->load_asset_t<SubScene>(scene_id);
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
	EXO_PROFILE_SCOPE;

	this->ui.painter->index_offset        = 0;
	this->ui.painter->vertex_bytes_offset = 0;
	ui::new_frame(this->ui);

	auto fullscreen_rect = Rect{.pos = {0, 0}, .size = float2(int2(this->window->size.x, this->window->size.y))};
	auto em              = this->ui.theme.font_size;

	docking::begin_docking(this->docking, this->ui, fullscreen_rect);

	char label_buf[256] = {};

	if (auto view_rect = docking::tabview(this->ui, this->docking, "View 1"); view_rect) {
		EXO_PROFILE_SCOPE_NAMED("Test 1");
		ui::label_in_rect(ui, view_rect.value(), "test");
	}

	if (auto view_rect = docking::tabview(this->ui, this->docking, "View 2"); view_rect) {
		EXO_PROFILE_SCOPE_NAMED("Test 2");
		ui::label_in_rect(ui, view_rect.value(), "test 2");
	}

	if (auto view_rect = docking::tabview(this->ui, this->docking, "Docking"); view_rect) {
		EXO_PROFILE_SCOPE_NAMED("Docking");

		auto content_rect = rect_inset(view_rect.value(), float2(1.0f * em));
		ui::push_clip_rect(this->ui, ui::register_clip_rect(this->ui, view_rect.value()));
		docking::inspector_ui(this->docking, this->ui, content_rect);
		ui::pop_clip_rect(this->ui);
	}

	if (auto view_rect = docking::tabview(this->ui, this->docking, "Outliner"); view_rect) {
		EXO_PROFILE_SCOPE_NAMED("Scene treeview");

		static auto scene_scroll_offset = float2();

		auto content_rect = rect_inset(view_rect.value(), float2(1.0f * em));
		ui::push_clip_rect(this->ui, ui::register_clip_rect(this->ui, content_rect));
		auto inner_content_rect = ui::begin_scroll_area(this->ui, content_rect, scene_scroll_offset);
		scene_treeview_ui(this->ui, this->scene, inner_content_rect);
		ui::end_scroll_area(this->ui, inner_content_rect);
		ui::pop_clip_rect(this->ui);
	}

	if (auto view_rect = docking::tabview(this->ui, this->docking, "Inspector"); view_rect) {
		EXO_PROFILE_SCOPE_NAMED("Scene inspector");

		static auto scene_inspector_scroll_offset = float2();

		auto content_rect = rect_inset(view_rect.value(), float2(1.0f * em));
		ui::push_clip_rect(this->ui, ui::register_clip_rect(this->ui, content_rect));
		auto inner_content_rect = ui::begin_scroll_area(this->ui, content_rect, scene_inspector_scroll_offset);
		scene_inspector_ui(this->ui, this->scene, inner_content_rect);
		ui::end_scroll_area(this->ui, inner_content_rect);
		ui::pop_clip_rect(this->ui);
	}

	if (auto view_rect = docking::tabview(this->ui, this->docking, "Inputs"); view_rect) {
		EXO_PROFILE_SCOPE_NAMED("Inputs");

		auto content_rect = rect_inset(view_rect.value(), float2(1.0f * em));
		ui::push_clip_rect(this->ui, ui::register_clip_rect(this->ui, content_rect));

		auto rectsplit = RectSplit{content_rect, SplitDirection::Top};

		ui::label_split(this->ui, rectsplit, fmt::format("Active: {}", this->ui.activation.active));
		ui::label_split(this->ui, rectsplit, fmt::format("Focused: {}", this->ui.activation.focused));
		rectsplit.split(1.0f * em);

		ui::label_split(this->ui, rectsplit, "Mouse buttons pressed:");
		for (auto pressed : this->inputs.mouse_buttons_pressed) {
			ui::label_split(this->ui, rectsplit, fmt::format("  {}", pressed));
		}
		rectsplit.split(1.0f * em);

		ui::label_split(this->ui, rectsplit, "Mouse wheel:");
		if (this->ui.inputs.mouse_wheel) {
			ui::label_split(this->ui,
				rectsplit,
				fmt::format("  {}x{}", this->ui.inputs.mouse_wheel.value().x, this->ui.inputs.mouse_wheel.value().y));
		} else {
			ui::label_split(this->ui, rectsplit, "  <none>");
		}

		ui::pop_clip_rect(this->ui);
	}

	if (auto view_rect = docking::tabview(this->ui, this->docking, "Asset Manager"); view_rect) {
		EXO_PROFILE_SCOPE_NAMED("Asset Manager");

		auto content_rect = rect_inset(view_rect.value(), float2(1.0f * em));
		ui::push_clip_rect(this->ui, ui::register_clip_rect(this->ui, content_rect));

		auto rectsplit = RectSplit{content_rect, SplitDirection::Top};

		// Resources
		static auto scroll_offset = float2();
		auto        res           = fmt::format_to_n(label_buf, 256, "Resources (offset {}):", scroll_offset.y);
		ui::label_split(this->ui, rectsplit, std::string_view{&label_buf[0], res.size});
		rectsplit.split(0.5f * em);

		auto &scrollarea_rect    = content_rect;
		auto  inner_content_rect = ui::begin_scroll_area(this->ui, scrollarea_rect, scroll_offset);
		auto  scroll_rectsplit   = RectSplit{inner_content_rect, SplitDirection::Top};
		for (auto [handle, p_resource] : this->asset_manager->database.resource_records) {
			if (p_resource->asset_id.is_valid()) {
				res = fmt::format_to_n(label_buf, 256, "name: \"{}\"", p_resource->asset_id.name);
			} else {
				res = fmt::format_to_n(label_buf, 256, "INVALID");
			}
			ui::label_split(this->ui, scroll_rectsplit, std::string_view{&label_buf[0], res.size});

			res = fmt::format_to_n(label_buf, 256, "path: \"{}\"", p_resource->resource_path.view());
			ui::label_split(this->ui, scroll_rectsplit, std::string_view{&label_buf[0], res.size});

			scroll_rectsplit.split(1.0f * em);
		}
		ui::end_scroll_area(this->ui, inner_content_rect);

		ui::pop_clip_rect(this->ui);
	}

	if (auto view_rect = docking::tabview(this->ui, this->docking, "Viewport"); view_rect) {
		EXO_PROFILE_SCOPE_NAMED("3D viewport");
		this->viewport_size = view_rect.value().size;

		if (this->viewport_texture_index != u32_invalid) {
			const Rect uv = {.pos = float2(0.0f), .size = float2(1.0f)};
			painter_draw_textured_rect(*this->painter,
				view_rect.value(),
				u32_invalid,
				uv,
				this->viewport_texture_index);
		}
	} else {
		this->viewport_size = float2(-1.0f);
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
		EXO_PROFILE_SWITCH_TO_FIBER("poll");
		window->poll_events();
		EXO_PROFILE_LEAVE_FIBER;

		EXO_PROFILE_SCOPE_NAMED("loop");

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
		this->ui.inputs.mouse_wheel                      = this->inputs.scroll_this_frame;

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
			const u64    now  = stm_now();
			const u64    diff = stm_diff(now, last);
			const double dt   = stm_sec(diff);

			last = now;
			this->display_ui(dt);
			this->scene.update(inputs);
			this->render_world =
				this->scene.entity_world.get_system_registry().get_system<PrepareRenderWorld>()->render_world;

			this->render_world.main_camera_projection = camera::infinite_perspective(this->render_world.main_camera_fov,
				this->viewport_size.x / this->viewport_size.y,
				0.1f);

			DrawInput draw_input           = {};
			draw_input.world_viewport_size = this->viewport_size;
			draw_input.world               = &this->render_world;
			draw_input.painter             = this->painter;
			auto draw_result               = this->renderer.draw(draw_input);

			this->painter->glyph_atlas_gpu_idx = draw_result.glyph_atlas_index;
			this->viewport_texture_index       = draw_result.scene_viewport_index;
		}

		watcher.update([&](const cross::Watch & /*watch*/, const cross::WatchEvent & /*event*/) {
			// this->asset_manager->on_file_change(watch, event);
		});

		EXO_PROFILE_FRAMEMARK;
	}
}
