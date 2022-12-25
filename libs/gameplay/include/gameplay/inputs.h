#pragma once

#include "cross/buttons.h"
#include "cross/events.h"
#include "exo/collections/vector.h"
#include "exo/maths/vectors.h"
#include "exo/option.h"

// clang-format off
namespace exo { struct Event; }
// clang-format on

enum struct Action : uint
{
#define X(display_name, enum) enum,
#include "input_bindings.def"
#undef X
	Count
};

inline constexpr exo::EnumArray<const char *, Action> action_to_string_array{
#define X(display_name, enum) display_name,
#include "input_bindings.def"
#undef X
};

inline constexpr const char *to_string(Action action) { return action_to_string_array[action]; }

struct KeyBinding
{
	// all keys need to be pressed
    exo::Vec<cross::VirtualKey>  keys;
    exo::Vec<cross::MouseButton> mouse_buttons;
};

class Inputs
{
public:
	void bind(Action action, KeyBinding &&binding);

	bool                     is_pressed(Action action) const;
    bool                     is_pressed(cross::VirtualKey Key) const;
    bool                     is_pressed(cross::MouseButton mouse_button) const;
	inline exo::Option<int2> get_scroll_this_frame() const { return scroll_this_frame; }
	inline exo::Option<int2> get_mouse_delta() const { return mouse_delta; }

    void        process(const exo::Vec<cross::Event> &events);
	inline void consume_scroll() { scroll_this_frame = {}; }

	exo::EnumArray<Option<KeyBinding>, Action> bindings;
    exo::EnumArray<bool, cross::VirtualKey>      keys_pressed          = {};
    exo::EnumArray<bool, cross::MouseButton>     mouse_buttons_pressed = {};

	exo::Option<int2> scroll_this_frame = {};
	exo::Option<int2> mouse_drag_start  = {};
	exo::Option<int2> mouse_drag_delta  = {};
	exo::Option<int2> mouse_delta       = {};
	int2              mouse_position    = {0};
	int2              main_window_size  = {};
};
