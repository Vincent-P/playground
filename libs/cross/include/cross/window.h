#pragma once
#include "cross/buttons.h"
#include "cross/events.h"
#include "exo/collections/enum_array.h"
#include "exo/collections/vector.h"
#include "exo/forward_container.h"
#include "exo/maths/vectors.h"

#include "exo/string.h"
#include "exo/string_view.h"
#include <memory>

namespace cross
{
enum struct Cursor : int
{
	None,
	Arrow,
	TextInput,
	ResizeAll,
	ResizeEW,
	ResizeNS,
	ResizeNESW,
	ResizeNWSE,
	Hand,
	NotAllowed
};

struct Window
{
	struct Impl;
	exo::ForwardContainer<Impl, 128> impl;

	exo::EnumArray<bool, exo::VirtualKey>  keys_pressed          = {};
	exo::EnumArray<bool, exo::MouseButton> mouse_buttons_pressed = {};

	Vec<exo::Event> events;

	exo::String title;
	int2        size;

	int2 mouse_position = int2{0, 0};

	Cursor current_cursor = Cursor::Arrow;

	bool has_focus{false};
	bool minimized{false};
	bool maximized{false};
	bool stop{false};

	// --
	static std::unique_ptr<Window> create(int2 size, const exo::StringView title);
	~Window() = default;

	void set_title(exo::StringView new_title);
	void poll_events();

	// cursor operations
	void set_cursor(Cursor cursor);

	[[nodiscard]] float2 get_dpi_scale() const;

	[[nodiscard]] inline bool should_close() const { return stop; }
	[[nodiscard]] inline bool is_key_pressed(exo::VirtualKey key) const { return keys_pressed[key]; }
	[[nodiscard]] inline bool is_mouse_button_pressed(exo::MouseButton button) const
	{
		return mouse_buttons_pressed[button];
	}
	[[nodiscard]] inline int2 get_mouse_position() const { return mouse_position; }
	u64                       get_win32_hwnd() const;
};

} // namespace cross
