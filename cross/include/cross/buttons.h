#pragma once
#include "exo/collections/enum_array.h"
#include "exo/maths/numerics.h"

namespace exo
{
enum struct MouseButton : uint
{
	Left,
	Right,
	Middle,
	SideForward,
	SideBackward,
	Count
};

inline constexpr EnumArray<const char *, MouseButton> mouse_button_to_string{
	"Left mouse button",
	"Right mouse button",
	"Middle mouse button (wheel)",
	"Side mouse button forward",
	"Side mouse button backward",
};

inline constexpr const char *to_string(MouseButton button) { return mouse_button_to_string[button]; }

enum struct VirtualKey : uint
{
#define X(EnumName, DisplayName, Win32, Xlib) EnumName,
#include "cross/keyboard_keys.def"
#undef X
	Count
};

inline constexpr EnumArray<const char *, VirtualKey> key_to_string{
#define X(EnumName, DisplayName, Win32, Xlib) DisplayName,
#include "cross/keyboard_keys.def"
#undef X
};

inline constexpr const char *to_string(VirtualKey key) { return key_to_string[key]; }

enum class ButtonState
{
	Pressed,
	Released
};

inline constexpr const char *to_string(ButtonState state)
{
	return state == ButtonState::Pressed ? "Pressed" : "Released";
}
} // namespace exo
