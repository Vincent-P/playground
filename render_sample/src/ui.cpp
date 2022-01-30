#include "ui.h"

#include <engine/inputs.h>
#include <exo/maths/pointer.h>
#include <exo/os/window.h>

#include "painter.h"
#include "rect.h"

bool ui_is_hovering(const UiState &ui_state, const Rect &rect) { return rect_is_point_inside(rect, exo::float2(ui_state.inputs->mouse_position)); }

u64 ui_make_id(UiState &state) { return ++state.gen; }

void ui_new_frame(UiState &ui_state)
{
	ui_state.gen     = 0;
	ui_state.focused = 0;
	ui_state.cursor  = static_cast<int>(exo::Cursor::Arrow);
}

void ui_end_frame(UiState &ui_state)
{
	if (!ui_state.inputs->mouse_buttons_pressed[exo::MouseButton::Left]) {
		ui_state.active = 0;
	} else {
		// active = invalid means no widget are focused
		if (ui_state.active == 0) {
			ui_state.active = u64_invalid;
		}
	}
}

u32 ui_register_clip_rect(UiState &ui_state, const Rect &clip_rect)
{
	painter_draw_color_rect(*ui_state.painter, clip_rect, u32_invalid, 0x88FF0000);

	auto i_first_rect_index                                   = ui_state.painter->index_offset - 6;
	ui_state.painter->indices[i_first_rect_index++].bits.type = RectType_Clip;
	ui_state.painter->indices[i_first_rect_index++].bits.type = RectType_Clip;
	ui_state.painter->indices[i_first_rect_index++].bits.type = RectType_Clip;
	ui_state.painter->indices[i_first_rect_index++].bits.type = RectType_Clip;
	ui_state.painter->indices[i_first_rect_index++].bits.type = RectType_Clip;
	ui_state.painter->indices[i_first_rect_index++].bits.type = RectType_Clip;

	usize last_color_rect_offset = ui_state.painter->vertex_bytes_offset - sizeof(ColorRect);

	ASSERT(last_color_rect_offset % sizeof(Rect) == 0);
	return static_cast<u32>(last_color_rect_offset / sizeof(Rect));
}

void ui_push_clip_rect(UiState &ui_state, u32 i_clip_rect)
{
	ASSERT(ui_state.i_clip_stack < UI_MAX_DEPTH);
	ui_state.clip_stack[ui_state.i_clip_stack] = i_clip_rect;
	ui_state.i_clip_stack += 1;
	ui_state.i_clip_rect = i_clip_rect;
}

void ui_pop_clip_rect(UiState &ui_state)
{
	ASSERT(0 < ui_state.i_clip_stack && ui_state.i_clip_stack < UI_MAX_DEPTH);
	ui_state.i_clip_stack -= 1;

	if (ui_state.i_clip_stack > 0) {
		ui_state.i_clip_rect = ui_state.clip_stack[ui_state.i_clip_stack - 1];
	} else {
		ui_state.i_clip_rect = u32_invalid;
	}
}

bool ui_button(UiState &ui_state, const UiTheme &ui_theme, const UiButton &button)
{
	bool result = false;
	u64  id     = ui_make_id(ui_state);

	ui_push_clip_rect(ui_state, ui_register_clip_rect(ui_state, button.rect));

	// behavior
	if (ui_is_hovering(ui_state, button.rect)) {
		ui_state.focused = id;
		if (ui_state.active == 0 && ui_state.inputs->mouse_buttons_pressed[exo::MouseButton::Left]) {
			ui_state.active = id;
		}
	}

	if (!ui_state.inputs->mouse_buttons_pressed[exo::MouseButton::Left] && ui_state.focused == id && ui_state.active == id) {
		result = true;
	}

	// rendering
	auto bg_color = ui_theme.button_bg_color;
	if (ui_state.focused == id) {
		if (ui_state.active == id) {
			bg_color = ui_theme.button_pressed_bg_color;
		} else {
			bg_color = ui_theme.button_hover_bg_color;
		}
	}
	painter_draw_color_rect(*ui_state.painter, button.rect, ui_state.i_clip_rect, bg_color);

	auto label_rect = rect_center(button.rect, exo::float2(measure_label(ui_theme.main_font, button.label)));
	// painter_draw_color_rect(*ui_state.painter, label_rect, u32_invalid, 0x880000FF);
	painter_draw_label(*ui_state.painter, label_rect, ui_state.i_clip_rect, ui_theme.main_font, button.label);

	ui_pop_clip_rect(ui_state);

	return result;
}

void ui_splitter_x(UiState &ui_state, const UiTheme &ui_theme, const Rect &view_rect, float &value, Rect &left, Rect &right)
{
	u64 id = ui_make_id(ui_state);

	left  = rect_split_off_left(view_rect, value * view_rect.size.x, ui_theme.splitter_thickness).left;
	right = rect_split_off_left(view_rect, value * view_rect.size.x, ui_theme.splitter_thickness).right;

	auto splitter_input = Rect{
		.position = view_rect.position + exo::float2{left.size.x - ui_theme.input_thickness / 2.0f, 0.0f},
		.size     = {ui_theme.input_thickness, view_rect.size.y},
	};

	// behavior
	if (ui_is_hovering(ui_state, splitter_input)) {
		ui_state.cursor  = static_cast<int>(exo::Cursor::ResizeEW);
		ui_state.focused = id;
		if (ui_state.active == 0 && ui_state.inputs->mouse_buttons_pressed[exo::MouseButton::Left]) {
			ui_state.active = id;
		}
	}

	if (ui_state.active == id) {
		value = (float(ui_state.inputs->mouse_position.x) - view_rect.position.x) / view_rect.size.x;
	}

	const auto color = ui_state.focused == id ? ui_theme.splitter_hover_color : ui_theme.splitter_color;
	auto rect_thickness = ui_state.focused == id ? ui_theme.splitter_hover_thickness : ui_theme.splitter_thickness;
	rect_thickness = 1.0f;
	painter_draw_color_rect(*ui_state.painter, {.position = {right.position.x - rect_thickness / 2.0f, view_rect.position.y}, .size = {rect_thickness, view_rect.size.y}}, ui_state.i_clip_rect, color);
}

void ui_splitter_y(UiState &ui_state, const UiTheme &ui_theme, const Rect &view_rect, float &value, Rect &top, Rect &bottom)
{
	u64 id = ui_make_id(ui_state);

	top    = rect_split_off_top(view_rect, value * view_rect.size.y, ui_theme.splitter_thickness).top;
	bottom = rect_split_off_top(view_rect, value * view_rect.size.y, ui_theme.splitter_thickness).bottom;

	auto splitter_input = Rect{
		.position = view_rect.position + exo::float2{0.0f, top.size.y - ui_theme.input_thickness / 2.0f},
		.size     = {view_rect.size.x, ui_theme.input_thickness},
	};

	// behavior
	if (ui_is_hovering(ui_state, splitter_input)) {
		ui_state.cursor  = static_cast<int>(exo::Cursor::ResizeNS);
		ui_state.focused = id;
		if (ui_state.active == 0 && ui_state.inputs->mouse_buttons_pressed[exo::MouseButton::Left]) {
			ui_state.active = id;
		}
	}

	if (ui_state.active == id) {
		value = (float(ui_state.inputs->mouse_position.y) - view_rect.position.y) / view_rect.size.y;
	}

	const auto color = ui_state.focused == id ? ui_theme.splitter_hover_color : ui_theme.splitter_color;
	auto rect_thickness = ui_state.focused == id ? ui_theme.splitter_hover_thickness : ui_theme.splitter_thickness;
	rect_thickness = 1.0f;
	painter_draw_color_rect(*ui_state.painter, {.position = {view_rect.position.x, bottom.position.y - rect_thickness / 2.0f}, .size = {view_rect.size.x, rect_thickness}}, ui_state.i_clip_rect, color);
}

void ui_label(UiState &ui_state, const UiTheme &ui_theme, const UiLabel &label) { painter_draw_label(*ui_state.painter, label.rect, ui_state.i_clip_rect, ui_theme.main_font, label.text); }

void ui_rect(UiState &ui_state, const UiTheme &ui_theme, const UiRect &rect)
{
    painter_draw_color_rect(*ui_state.painter, rect.rect, ui_state.i_clip_rect, rect.color);
}
