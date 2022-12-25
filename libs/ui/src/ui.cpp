#include "ui/ui.h"
#include "cross/window.h"
#include "exo/collections/span.h"
#include "exo/maths/pointer.h"
#include "exo/profile.h"
#include "painter/painter.h"
#include "painter/rect.h"

namespace ui
{
Ui Ui::create(Font *font, float font_size, Painter *painter)
{
	Ui ui = {};
	ui.theme.main_font = font;
	ui.theme.font_size = font_size;
	ui.painter = painter;
	return ui;
}

void Ui::new_frame()
{
	EXO_PROFILE_SCOPE;

	this->activation.gen = 0;
	this->activation.focused = u64_invalid;
	this->state.cursor = static_cast<int>(cross::Cursor::Arrow);
}

void Ui::end_frame()
{
	EXO_PROFILE_SCOPE;

	if (!this->inputs.mouse_buttons_pressed[cross::MouseButton::Left]) {
		this->activation.active = u64_invalid;
	}
}

bool Ui::is_hovering(const Rect &rect) const { return rect.is_point_inside(this->mouse_position()); }

u64 Ui::make_id() { return ++this->activation.gen; }

bool Ui::has_pressed(cross::MouseButton button) const { return this->inputs.mouse_buttons_pressed[button]; }

bool Ui::has_pressed_and_released(cross::MouseButton button) const
{
	return !this->inputs.mouse_buttons_pressed_last_frame[button] && this->inputs.mouse_buttons_pressed[button];
}

bool Ui::has_clicked(u64 id, cross::MouseButton button) const
{
	return this->has_pressed_and_released(button) && this->activation.focused == id && this->activation.active == id;
}

const Rect &Ui::current_clip_rect() const
{
	return exo::reinterpret_span<Rect>(this->painter->vertex_buffer)[this->state.current_clip_rect];
}

bool Ui::is_clipped(const Rect &rect) const { return !rect.intersects(this->current_clip_rect()); }

u32 Ui::register_clip_rect(const Rect &clip_rect)
{
	EXO_PROFILE_SCOPE;

	this->painter->draw_color_rect(clip_rect, u32_invalid, ColorU32::from_uints(0, 0, 0xFF, 0x88));

	auto i_first_rect_index = this->painter->index_offset - 6;
	this->painter->index_buffer[i_first_rect_index++].bits.type = RectType_Clip;
	this->painter->index_buffer[i_first_rect_index++].bits.type = RectType_Clip;
	this->painter->index_buffer[i_first_rect_index++].bits.type = RectType_Clip;
	this->painter->index_buffer[i_first_rect_index++].bits.type = RectType_Clip;
	this->painter->index_buffer[i_first_rect_index++].bits.type = RectType_Clip;
	this->painter->index_buffer[i_first_rect_index++].bits.type = RectType_Clip;

	const usize last_color_rect_offset = this->painter->vertex_bytes_offset - sizeof(ColorRect);
	ASSERT(last_color_rect_offset % sizeof(Rect) == 0);
	return static_cast<u32>(last_color_rect_offset / sizeof(Rect));
}

void Ui::push_clip_rect(u32 i_clip_rect)
{
	this->state.clip_stack.push(i_clip_rect);
	this->state.current_clip_rect = i_clip_rect;
}

void Ui::pop_clip_rect()
{
	this->state.clip_stack.pop();

	if (!this->state.clip_stack.is_empty()) {
		this->state.current_clip_rect = this->state.clip_stack.last();
	} else {
		this->state.current_clip_rect = u32_invalid;
	}
}

bool button(Ui &ui, const Button &button)
{
	EXO_PROFILE_SCOPE;

	bool result = false;
	const u64 id = ui.make_id();

	ui.push_clip_rect(ui.register_clip_rect(button.rect));

	// behavior
	if (ui.is_hovering(button.rect)) {
		ui.activation.focused = id;
		if (ui.activation.active == u64_invalid && ui.inputs.mouse_buttons_pressed[cross::MouseButton::Left]) {
			ui.activation.active = id;
		}
	}

	if (ui.has_clicked(id)) {
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
	ui.painter->draw_color_round_rect(button.rect,
		ui.state.current_clip_rect,
		bg_color,
		ColorU32::from_uints(0, 0, 0, 0x0F),
		2);

	auto label_rect = button.rect.center(float2(ui.painter->measure_label(*ui.theme.main_font, button.label)));

	ui.painter->draw_label(label_rect, ui.state.current_clip_rect, *ui.theme.main_font, button.label);

	ui.pop_clip_rect();

	return result;
}

RectPair splitter_x(Ui &ui, const Rect &view_rect, float &value)
{
	EXO_PROFILE_SCOPE;

	const u64 id = ui.make_id();

	Rect right_rect = view_rect;
	Rect left_rect = right_rect.split_left(value * view_rect.size.x);
	Rect splitter_rect = right_rect.split_left(ui.theme.splitter_thickness);

	// behavior
	if (ui.is_hovering(splitter_rect)) {
		ui.activation.focused = id;
		if (ui.activation.active == u64_invalid && ui.inputs.mouse_buttons_pressed[cross::MouseButton::Left]) {
			ui.activation.active = id;
		}
	}

	if (ui.activation.focused == id || ui.activation.active == id) {
		ui.state.cursor = static_cast<int>(cross::Cursor::ResizeEW);
	}

	if (ui.activation.active == id) {
		value = (float(ui.inputs.mouse_position.x) - view_rect.pos.x) / view_rect.size.x;
	}

	// rendering
	const auto color = ui.activation.focused == id ? ui.theme.splitter_hover_color : ui.theme.splitter_color;
	if (ui.activation.focused == id) {
		const float thickness_difference = ui.theme.splitter_hover_thickness - ui.theme.splitter_thickness;
		splitter_rect = splitter_rect.outset(float2(thickness_difference, 0.0f));
	}
	ui.painter->draw_color_rect(splitter_rect, ui.state.current_clip_rect, color);

	return {left_rect, right_rect};
}

RectPair splitter_y(Ui &ui, const Rect &view_rect, float &value)
{
	EXO_PROFILE_SCOPE;

	const u64 id = ui.make_id();

	Rect bottom_rect = view_rect;
	Rect top_rect = bottom_rect.split_top(value * view_rect.size.y);
	Rect splitter_rect = bottom_rect.split_top(ui.theme.splitter_thickness);

	// behavior
	if (ui.is_hovering(splitter_rect)) {
		ui.activation.focused = id;
		if (ui.activation.active == u64_invalid && ui.inputs.mouse_buttons_pressed[cross::MouseButton::Left]) {
			ui.activation.active = id;
		}
	}

	if (ui.activation.focused == id || ui.activation.active == id) {
		ui.state.cursor = static_cast<int>(cross::Cursor::ResizeNS);
	}

	if (ui.activation.active == id) {
		value = (float(ui.inputs.mouse_position.y) - view_rect.pos.y) / view_rect.size.y;
	}

	// rendering
	const auto color = ui.activation.focused == id ? ui.theme.splitter_hover_color : ui.theme.splitter_color;
	if (ui.activation.focused == id) {
		const float thickness_difference = ui.theme.splitter_hover_thickness - ui.theme.splitter_thickness;
		splitter_rect = splitter_rect.outset(float2(0.0f, thickness_difference));
	}
	ui.painter->draw_color_rect(splitter_rect, ui.state.current_clip_rect, color);

	return {top_rect, bottom_rect};
}

void label_in_rect(Ui &ui, const Rect &view_rect, exo::StringView label, Alignment alignment)
{
	ASSERT(alignment == Alignment::Center);
	auto label_rect = view_rect.center(float2(ui.painter->measure_label(*ui.theme.main_font, label)));

	ui.push_clip_rect(ui.register_clip_rect(view_rect));
	ui.painter->draw_label(label_rect, ui.state.current_clip_rect, *ui.theme.main_font, label);
	ui.pop_clip_rect();
}

Rect label_split(Ui &ui, RectSplit &rectsplit, exo::StringView label)
{
	EXO_PROFILE_SCOPE;

	auto label_size = ui.painter->measure_label(*ui.theme.main_font, label);
	auto line_rect = rectsplit.split(float2(label_size));

	if (ui.is_clipped(line_rect)) {
		return line_rect;
	}

	ui.painter->draw_label(line_rect, ui.state.current_clip_rect, *ui.theme.main_font, label);
	return line_rect;
}

bool button_split(Ui &ui, RectSplit &rectsplit, exo::StringView label)
{
	EXO_PROFILE_SCOPE;

	bool result = false;
	const u64 id = ui.make_id();

	// layout
	auto label_size = ui.painter->measure_label(*ui.theme.main_font, label);
	auto button_rect = rectsplit.split(float(label_size.x) + 0.5f * ui.theme.font_size);
	auto label_rect = button_rect.center(float2(ui.painter->measure_label(*ui.theme.main_font, label)));

	// behavior
	if (ui.is_hovering(button_rect)) {
		ui.activation.focused = id;
		if (ui.activation.active == u64_invalid && ui.inputs.mouse_buttons_pressed[cross::MouseButton::Left]) {
			ui.activation.active = id;
		}
	}

	if (ui.has_clicked(id)) {
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
	ui.painter->draw_color_round_rect(button_rect,
		ui.state.current_clip_rect,
		bg_color,
		ColorU32::from_uints(0, 0, 0, 0x0F),
		2);

	ui.painter->draw_label(label_rect, ui.state.current_clip_rect, *ui.theme.main_font, label);

	return result;
}

bool invisible_button(Ui &ui, const Rect &rect)
{
	EXO_PROFILE_SCOPE;

	bool result = false;
	const u64 id = ui.make_id();

	// behavior
	if (ui.is_hovering(rect)) {
		ui.activation.focused = id;
		if (ui.activation.active == u64_invalid && ui.inputs.mouse_buttons_pressed[cross::MouseButton::Left]) {
			ui.activation.active = id;
		}
	}

	if (ui.has_clicked(id)) {
		result = true;
	}

	return result;
}

} // namespace ui
