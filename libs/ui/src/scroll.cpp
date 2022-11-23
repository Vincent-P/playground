#include "ui/scroll.h"

#include "painter/painter.h"
#include "ui/ui.h"

namespace ui
{
inline constexpr float2 MAX_SCROLL_SIZE = float2(65536.0f);

Rect begin_scroll_area(Ui &ui, const Rect &scrollview_rect, exo::float2 &offset)
{
	auto area_id = ui.make_id();
	auto id = ui.make_id();

	auto em = ui.theme.font_size;

	// layout
	ui.state.scroll_id_stack.push(id);
	auto *starting_rect = ui.state.scroll_starting_rects.at(id);
	if (!starting_rect) {
		starting_rect = ui.state.scroll_starting_rects.insert(id, {});
	}
	auto *ending_rect = ui.state.scroll_ending_rects.at(id);
	if (!ending_rect) {
		ending_rect = ui.state.scroll_ending_rects.insert(id, {});
	}

	*starting_rect = Rect{
		.pos = scrollview_rect.pos - offset,
		.size = MAX_SCROLL_SIZE,
	};

	auto actual_content_rect = Rect{
		.pos = starting_rect->pos,
		.size = ending_rect->pos - starting_rect->pos,
	};

	auto scroll_area_rect = scrollview_rect;
	auto right_vertical_scrollbar_rect = scroll_area_rect.split_right(1.0f * em);

	float2 offset_percent = offset / actual_content_rect.size;
	auto vertical_thumb = Rect{};
	vertical_thumb.pos =
		right_vertical_scrollbar_rect.pos + float2(0.0f, offset_percent.y * right_vertical_scrollbar_rect.size.y);
	vertical_thumb.size = {right_vertical_scrollbar_rect.size.x,
		(scrollview_rect.size.y / actual_content_rect.size.y) * right_vertical_scrollbar_rect.size.y};

	// interaction
	if (ui.is_hovering(scroll_area_rect)) {
		ui.activation.focused = area_id;
	}

	if (ui.is_hovering(vertical_thumb)) {
		ui.activation.focused = id;
		if (ui.activation.active == u64_invalid && ui.inputs.mouse_buttons_pressed[exo::MouseButton::Left]) {
			ui.activation.active = id;
			ui.state.active_drag_offset = ui.mouse_position() - vertical_thumb.pos;
		}
	}

	if (ui.activation.focused == id || ui.activation.focused == area_id) {
		if (ui.inputs.mouse_wheel) {
			offset = offset + em * float2(ui.inputs.mouse_wheel.value());
		}
	}

	if (ui.activation.active == id) {
		auto mouse_delta = ui.mouse_position() - ui.state.active_drag_offset;
		offset_percent = (mouse_delta - right_vertical_scrollbar_rect.pos) / right_vertical_scrollbar_rect.size;
		offset.y = offset_percent.y * actual_content_rect.size.y;
	}

	offset = exo::round(offset);

	// draw
	ui.push_clip_rect(ui.register_clip_rect(scrollview_rect));

	ui.painter->draw_color_rect(scrollview_rect, ui.state.current_clip_rect, ui.theme.scroll_area_bg_color);
	ui.painter->draw_color_rect(right_vertical_scrollbar_rect,
		ui.state.current_clip_rect,
		ui.theme.scroll_bar_bg_color);
	ui.painter->draw_color_rect(vertical_thumb, ui.state.current_clip_rect, ui.theme.scroll_thumb_bg_color);

	return *starting_rect;
}

void end_scroll_area(Ui &ui, const Rect &new_ending_rect)
{
	auto id = ui.state.scroll_id_stack.pop();
	auto *ending_rect = ui.state.scroll_ending_rects.at(id);
	if (!ending_rect) {
		ending_rect = ui.state.scroll_ending_rects.insert(id, {});
	}
	*ending_rect = new_ending_rect;

	ui.pop_clip_rect();
}
} // namespace ui
