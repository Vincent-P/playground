#include "ui/scroll.h"

#include "ui/ui.h"
#include <painter/painter.h>

namespace ui
{
inline constexpr float2 MAX_SCROLL_SIZE = float2(65536.0f);

Rect begin_scroll_area(Ui &ui, const Rect &content_rect, exo::float2 &offset)
{
	auto em = ui.theme.font_size;

	// layout
	ASSERT(ui.state.i_scroll_stack < UI_MAX_DEPTH);
	auto       &starting_rect = ui.state.scroll_starting_stack[ui.state.i_scroll_stack];
	const auto &ending_rect   = ui.state.scroll_ending_stack[ui.state.i_scroll_stack];
	auto       &scroll_delta  = ui.state.scroll_mouse_delta[ui.state.i_scroll_stack];
	ui.state.i_scroll_stack += 1;

	starting_rect = Rect{
		.pos  = content_rect.pos - offset,
		.size = MAX_SCROLL_SIZE,
	};

	auto actual_content_rect = Rect{
		.pos  = starting_rect.pos,
		.size = ending_rect.pos - starting_rect.pos,
	};
	float2 offset_percent = offset / actual_content_rect.size;

	auto scroll_area_rect              = content_rect;
	auto right_vertical_scrollbar_rect = rect_split_right(scroll_area_rect, 1.0f * em);

	auto vertical_thumb = Rect{
		.pos  = right_vertical_scrollbar_rect.pos + float2(0.0f, offset_percent.y * right_vertical_scrollbar_rect.size.y),
		.size = {right_vertical_scrollbar_rect.size.x,
			(content_rect.size.y / actual_content_rect.size.y) * right_vertical_scrollbar_rect.size.y},
	};

	// interaction
	auto id = make_id(ui);
	if (is_hovering(ui, vertical_thumb)) {
		ui.activation.focused = id;
		if (ui.activation.active == u64_invalid && ui.inputs.mouse_buttons_pressed[exo::MouseButton::Left]) {
			ui.activation.active = id;
			scroll_delta         = mouse_position(ui) - vertical_thumb.pos;
		}
	}

	if (ui.activation.active == id) {
		auto mouse_offset_percentage =
			(mouse_position(ui).y - scroll_delta.y - right_vertical_scrollbar_rect.pos.y) / right_vertical_scrollbar_rect.pos.y;
		offset.y = mouse_offset_percentage * actual_content_rect.size.y;
	}

	offset = exo::floor(offset);
	offset = exo::max(exo::min(offset, actual_content_rect.size), float2(0.0f));

	// draw
	push_clip_rect(ui, register_clip_rect(ui, content_rect));

	painter_draw_color_rect(*ui.painter, content_rect, ui.state.i_clip_rect, ColorU32::green());
	painter_draw_color_rect(*ui.painter, right_vertical_scrollbar_rect, ui.state.i_clip_rect, ColorU32::red());
	painter_draw_color_rect(*ui.painter, vertical_thumb, ui.state.i_clip_rect, ColorU32::blue());

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
