#include "ui/scroll.h"

#include "painter/painter.h"
#include "ui/ui.h"

namespace ui
{
inline constexpr float2 MAX_SCROLL_SIZE = float2(65536.0f);

Rect begin_scroll_area(Ui &ui, const Rect &scrollview_rect, exo::float2 &offset)
{
	auto em = ui.theme.font_size;

	// layout
	ASSERT(ui.state.i_scroll_stack < UI_MAX_DEPTH);
	auto       &starting_rect = ui.state.scroll_starting_stack[ui.state.i_scroll_stack];
	const auto &ending_rect   = ui.state.scroll_ending_stack[ui.state.i_scroll_stack];
	ui.state.i_scroll_stack += 1;

	starting_rect = Rect{
		.pos  = scrollview_rect.pos - offset,
		.size = MAX_SCROLL_SIZE,
	};

	auto actual_content_rect = Rect{
		.pos  = starting_rect.pos,
		.size = ending_rect.pos - starting_rect.pos,
	};

	auto scroll_area_rect              = scrollview_rect;
	auto right_vertical_scrollbar_rect = rect_split_right(scroll_area_rect, 1.0f * em);

	float2 offset_percent = offset / actual_content_rect.size;
	auto   vertical_thumb = Rect{};
	vertical_thumb.pos =
		right_vertical_scrollbar_rect.pos + float2(0.0f, offset_percent.y * right_vertical_scrollbar_rect.size.y);
	vertical_thumb.size = {right_vertical_scrollbar_rect.size.x,
		(scrollview_rect.size.y / actual_content_rect.size.y) * right_vertical_scrollbar_rect.size.y};

	// interaction
	auto area_id = make_id(ui);
	auto id      = make_id(ui);

	if (is_hovering(ui, scroll_area_rect)) {
		ui.activation.focused = area_id;
	}

	if (is_hovering(ui, vertical_thumb)) {
		ui.activation.focused = id;
		if (ui.activation.active == u64_invalid && ui.inputs.mouse_buttons_pressed[exo::MouseButton::Left]) {
			ui.activation.active        = id;
			ui.state.active_drag_offset = mouse_position(ui) - vertical_thumb.pos;
		}
	}

	if (ui.activation.focused == id || ui.activation.focused == area_id) {
		if (ui.inputs.mouse_wheel) {
			offset = offset + em * float2(ui.inputs.mouse_wheel.value());
		}
	}

	if (ui.activation.active == id) {
		auto mouse_delta = mouse_position(ui) - ui.state.active_drag_offset;
		offset_percent   = (mouse_delta - right_vertical_scrollbar_rect.pos) / right_vertical_scrollbar_rect.size;
		offset.y         = offset_percent.y * actual_content_rect.size.y;
	}

	offset = exo::round(offset);

	// draw
	push_clip_rect(ui, register_clip_rect(ui, scrollview_rect));

	ui.painter->draw_color_rect(scrollview_rect, ui.state.i_clip_rect, ui.theme.scroll_area_bg_color);
	ui.painter->draw_color_rect(right_vertical_scrollbar_rect, ui.state.i_clip_rect, ui.theme.scroll_bar_bg_color);
	ui.painter->draw_color_rect(vertical_thumb, ui.state.i_clip_rect, ui.theme.scroll_thumb_bg_color);

	return ui.state.scroll_starting_stack[ui.state.i_scroll_stack - 1];
}

void end_scroll_area(Ui &ui, const Rect &ending_rect)
{
	ASSERT(ui.state.i_scroll_stack > 0);
	ui.state.scroll_ending_stack[ui.state.i_scroll_stack - 1] = ending_rect;
	ui.state.i_scroll_stack -= 1;

	pop_clip_rect(ui);
}
} // namespace ui
