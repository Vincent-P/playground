#include "inputs.h"

#include "ui.h"

#include <cross/events.h>
#include <exo/option.h>

#include <algorithm>

void Inputs::bind(Action action, const KeyBinding &binding) { bindings[action] = binding; }

bool Inputs::is_pressed(Action action) const
{
	if (bindings[action].has_value()) {
		const auto &binding = this->bindings[action].value();
		return std::ranges::all_of(binding.keys, [&](exo::VirtualKey key) { return is_pressed(key); }) &&
		       std::ranges::all_of(binding.mouse_buttons, [&](exo::MouseButton button) { return is_pressed(button); });
	}

	return false;
}

bool Inputs::is_pressed(exo::VirtualKey key) const { return keys_pressed[key]; }

bool Inputs::is_pressed(exo::MouseButton button) const { return mouse_buttons_pressed[button]; }

void Inputs::process(const Vec<exo::Event> &events)
{
	scroll_this_frame        = std::nullopt;
	auto last_mouse_position = mouse_position;

	for (const auto &event : events) {
		if (event.type == exo::Event::KeyType) {
			const auto &key       = event.key;
			keys_pressed[key.key] = key.state == exo::ButtonState::Pressed;
		} else if (event.type == exo::Event::MouseClickType) {
			const auto &mouse_click                   = event.mouse_click;
			mouse_buttons_pressed[mouse_click.button] = mouse_click.state == exo::ButtonState::Pressed;

			if (mouse_click.button == exo::MouseButton::Left) {
				if (mouse_click.state == exo::ButtonState::Pressed) {
					if (!mouse_drag_start) {
						mouse_drag_start = mouse_position;
					}
				} else {
					mouse_drag_delta = std::nullopt;
					mouse_drag_start = std::nullopt;
				}
			}
		} else if (event.type == exo::Event::ScrollType) {
			const auto &scroll = event.scroll;

			if (scroll_this_frame) {
				scroll_this_frame->x += scroll.dx;
				scroll_this_frame->y += scroll.dy;
			} else {
				scroll_this_frame = {scroll.dx, scroll.dy};
			}
		} else if (event.type == exo::Event::MouseMoveType) {
			const auto &move    = event.mouse_move;
			last_mouse_position = {move.x, move.y};
		}
	}

	if (last_mouse_position.x != mouse_position.x || last_mouse_position.y != mouse_position.y) {
		mouse_delta    = {last_mouse_position.x - mouse_position.x, last_mouse_position.y - mouse_position.y};
		mouse_position = {last_mouse_position.x, last_mouse_position.y};

		if (mouse_drag_start) {
			mouse_drag_delta = mouse_position - *mouse_drag_start;
		}
	} else {
		mouse_delta = std::nullopt;
	}
}
