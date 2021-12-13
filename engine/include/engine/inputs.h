#pragma once

#include <exo/option.h>
#include <exo/maths/vectors.h>
#include <exo/collections/vector.h>
#include <exo/collections/map.h>
#include <exo/os/buttons.h>

#include <optional>
#include <string>

// clang-format off
namespace UI { struct Context; }
namespace exo::os { struct Event; }
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
    Vec<os::VirtualKey> keys;
    Vec<os::MouseButton> mouse_buttons;
};

class Inputs
{
  public:
    void bind(Action action, const KeyBinding &binding);

    bool is_pressed(Action action) const;
    bool is_pressed(os::VirtualKey Key) const;
    bool is_pressed(os::MouseButton mouse_button) const;
    inline Option<int2> get_scroll_this_frame() const { return scroll_this_frame; }
    inline Option<int2> get_mouse_delta() const { return mouse_delta; }

    void process(const Vec<os::Event> &events);
    inline void consume_scroll() { scroll_this_frame = {}; }

    void display_ui();

  private:
    exo::Map<Action, KeyBinding> bindings;

    exo::EnumArray<bool, os::VirtualKey> keys_pressed           = {};
    exo::EnumArray<bool, os::MouseButton> mouse_buttons_pressed = {};

    Option<int2> scroll_this_frame = {};
    Option<int2> mouse_drag_start  = {};
    Option<int2> mouse_drag_delta  = {};
    Option<int2> mouse_delta       = {};
    int2         mouse_position    = {0};

    friend UI::Context;
};
