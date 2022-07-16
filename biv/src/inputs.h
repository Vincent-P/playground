#pragma once

#include <exo/maths/vectors.h>
#include <exo/collections/vector.h>
#include <exo/collections/map.h>
#include <exo/option.h>
#include <exo/buttons.h>

#include <optional>
#include <string>

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
    exo::Vec<exo::VirtualKey> keys;
    exo::Vec<exo::MouseButton> mouse_buttons;
};

class Inputs
{
  public:
    void bind(Action action, const KeyBinding &binding);

    bool is_pressed(Action action) const;
    bool is_pressed(exo::VirtualKey Key) const;
    bool is_pressed(exo::MouseButton mouse_button) const;
    inline exo::Option<int2> get_scroll_this_frame() const { return scroll_this_frame; }
    inline exo::Option<int2> get_mouse_delta() const { return mouse_delta; }

    void process(const exo::Vec<exo::Event> &events);
    inline void consume_scroll() { scroll_this_frame = {}; }

    void display_ui();

    exo::Map<Action, KeyBinding> bindings;

    exo::EnumArray<bool, exo::VirtualKey> keys_pressed           = {};
    exo::EnumArray<bool, exo::MouseButton> mouse_buttons_pressed = {};

    exo::Option<int2> scroll_this_frame = {};
    exo::Option<int2> mouse_drag_start  = {};
    exo::Option<int2> mouse_drag_delta  = {};
    exo::Option<int2> mouse_delta       = {};
    int2         mouse_position    = {0};
};
