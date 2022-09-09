#include "ui/ui.h"

#include <cross/window.h>
#include <exo/maths/pointer.h>

#include "painter/painter.h"
#include "painter/rect.h"

namespace ui
{
bool is_hovering(const Ui &ui, const Rect &rect) { return rect_is_point_inside(rect, mouse_position(ui)); }

bool has_clicked(const Ui &ui, u64 id)
{
	return !ui.inputs.mouse_buttons_pressed[exo::MouseButton::Left] && ui.activation.focused == id &&
	       ui.activation.active == id;
}

u64 make_id(Ui &ui) { return ++ui.activation.gen; }

Ui create(Font *font, float font_size, Painter *painter)
{
	Ui ui;
	ui.theme.main_font = font;
	ui.theme.font_size = font_size;
	ui.painter         = painter;
	return ui;
}

void new_frame(Ui &ui)
{
	ui.activation.gen     = 0;
	ui.activation.focused = 0;
	ui.state.cursor       = static_cast<int>(cross::Cursor::Arrow);
}

void end_frame(Ui &ui)
{
	if (!ui.inputs.mouse_buttons_pressed[exo::MouseButton::Left]) {
		ui.activation.active = 0;
	} else {
		// active = invalid means no widget are focused
		if (ui.activation.active == 0) {
			ui.activation.active = u64_invalid;
		}
	}
}

u32 register_clip_rect(Ui &ui, const Rect &clip_rect)
{
	auto &painter                = *ui.painter;
	usize last_color_rect_offset = painter.vertex_bytes_offset;
	painter_draw_color_rect(painter, clip_rect, u32_invalid, ColorU32::from_uints(0, 0, 0xFF, 0x88));

	auto i_first_rect_index                         = painter.index_offset - 6;
	painter.indices[i_first_rect_index++].bits.type = RectType_Clip;
	painter.indices[i_first_rect_index++].bits.type = RectType_Clip;
	painter.indices[i_first_rect_index++].bits.type = RectType_Clip;
	painter.indices[i_first_rect_index++].bits.type = RectType_Clip;
	painter.indices[i_first_rect_index++].bits.type = RectType_Clip;
	painter.indices[i_first_rect_index++].bits.type = RectType_Clip;


	ASSERT(last_color_rect_offset % sizeof(Rect) == 0);
	return static_cast<u32>(last_color_rect_offset / sizeof(Rect));
}

void push_clip_rect(Ui &ui, u32 i_clip_rect)
{
	ASSERT(ui.state.i_clip_stack < UI_MAX_DEPTH);
	ui.state.clip_stack[ui.state.i_clip_stack] = i_clip_rect;
	ui.state.i_clip_stack += 1;
	ui.state.i_clip_rect = i_clip_rect;
}

void pop_clip_rect(Ui &ui)
{
	ASSERT(0 < ui.state.i_clip_stack && ui.state.i_clip_stack < UI_MAX_DEPTH);
	ui.state.i_clip_stack -= 1;

	if (ui.state.i_clip_stack > 0) {
		ui.state.i_clip_rect = ui.state.clip_stack[ui.state.i_clip_stack - 1];
	} else {
		ui.state.i_clip_rect = u32_invalid;
	}
}

bool button(Ui &ui, const Button &button)
{
	bool result = false;
	u64  id     = make_id(ui);

	push_clip_rect(ui, register_clip_rect(ui, button.rect));

	// behavior
	if (is_hovering(ui, button.rect)) {
		ui.activation.focused = id;
		if (ui.activation.active == 0 && ui.inputs.mouse_buttons_pressed[exo::MouseButton::Left]) {
			ui.activation.active = id;
		}
	}

	if (has_clicked(ui, id)) {
		result = true;
	}

	// rendering
	auto bg_color = ui.theme.button_bg_color;
	if (ui.activation.focused == id) {
		if (ui.activation.active == id) {
			bg_color = ui.theme.button_pressed_bg_color;
		} else {
			bg_color = ui.theme.button_hover_bg_color;
		}
	}
	painter_draw_color_round_rect(*ui.painter,
		button.rect,
		ui.state.i_clip_rect,
		bg_color,
		ColorU32::from_uints(0, 0, 0, 0x0F),
		2);

	auto label_rect = rect_center(button.rect, float2(measure_label(*ui.painter, *ui.theme.main_font, button.label)));

	painter_draw_label(*ui.painter, label_rect, ui.state.i_clip_rect, *ui.theme.main_font, button.label);

	pop_clip_rect(ui);

	return result;
}

void splitter_x(Ui &ui, const Rect &view_rect, float &value)
{
	u64 id = make_id(ui);

	Rect rect          = view_rect;
	Rect splitter_rect = rect_split_left(rect, ui.theme.splitter_thickness);

	// behavior
	if (is_hovering(ui, splitter_rect)) {
		ui.state.cursor       = static_cast<int>(cross::Cursor::ResizeEW);
		ui.activation.focused = id;
		if (ui.activation.active == 0 && ui.inputs.mouse_buttons_pressed[exo::MouseButton::Left]) {
			ui.activation.active = id;
		}
	}

	if (ui.activation.active == id) {
		value = (float(ui.inputs.mouse_position.x) - view_rect.pos.x) / view_rect.size.x;
	}

	const auto color = ui.activation.focused == id ? ui.theme.splitter_hover_color : ui.theme.splitter_color;
	if (ui.activation.focused == id) {
		float thickness_difference = ui.theme.splitter_hover_thickness - ui.theme.splitter_thickness;
		splitter_rect              = rect_outset(splitter_rect, float2(thickness_difference, 0.0f));
	}
	painter_draw_color_rect(*ui.painter, splitter_rect, ui.state.i_clip_rect, color);
}

void splitter_y(Ui &ui, const Rect &view_rect, float &value)
{
	u64 id = make_id(ui);

	Rect rect          = view_rect;
	Rect splitter_rect = rect_split_top(rect, ui.theme.splitter_thickness);

	// behavior
	if (is_hovering(ui, splitter_rect)) {
		ui.state.cursor       = static_cast<int>(cross::Cursor::ResizeNS);
		ui.activation.focused = id;
		if (ui.activation.active == 0 && ui.inputs.mouse_buttons_pressed[exo::MouseButton::Left]) {
			ui.activation.active = id;
		}
	}

	if (ui.activation.active == id) {
		value = (float(ui.inputs.mouse_position.y) - view_rect.pos.y) / view_rect.size.y;
	}

	const auto color = ui.activation.focused == id ? ui.theme.splitter_hover_color : ui.theme.splitter_color;
	if (ui.activation.focused == id) {
		float thickness_difference = ui.theme.splitter_hover_thickness - ui.theme.splitter_thickness;
		splitter_rect              = rect_outset(splitter_rect, float2(0.0f, thickness_difference));
	}
	painter_draw_color_rect(*ui.painter, splitter_rect, ui.state.i_clip_rect, color);
}

void label(Ui &ui, const Label &label)
{
	auto label_rect = rect_center(label.rect, float2(measure_label(*ui.painter, *ui.theme.main_font, label.text)));

	push_clip_rect(ui, register_clip_rect(ui, label.rect));
	painter_draw_label(*ui.painter, label_rect, ui.state.i_clip_rect, *ui.theme.main_font, label.text);
	pop_clip_rect(ui);
}

} // namespace ui
