#pragma once

#include <exo/base/option.h>
#include <exo/maths/vectors.h>
#include <exo/collections/vector.h>
#include <exo/collections/map.h>
#include <exo/cross/buttons.h>

#include <optional>
#include <string>

// clang-format off
namespace UI { struct Context; }
namespace cross { struct Event; }
// clang-format on

enum struct Action : uint
{
#define X(display_name, enum) enum,
#include "input_bindings.def"
#undef X
    Count
};

inline constexpr EnumArray<const char *, Action> action_to_string_array{
#define X(display_name, enum) display_name,
#include "input_bindings.def"
#undef X
};

inline constexpr const char *to_string(Action action) { return action_to_string_array[action]; }

struct KeyBinding
{
    // all keys need to be pressed
    Vec<cross::VirtualKey> keys;
    Vec<cross::MouseButton> mouse_buttons;
};

class Inputs
{
  public:
    void bind(Action action, const KeyBinding &binding);

    bool is_pressed(Action action) const;
    bool is_pressed(cross::VirtualKey Key) const;
    bool is_pressed(cross::MouseButton mouse_button) const;
    inline Option<int2> get_scroll_this_frame() const { return scroll_this_frame; }
    inline Option<int2> get_mouse_delta() const { return mouse_delta; }

    void process(const Vec<cross::Event> &events);
    inline void consume_scroll() { scroll_this_frame = {}; }

    void display_ui();

  private:
    Map<Action, KeyBinding> bindings;

    EnumArray<bool, cross::VirtualKey> keys_pressed           = {};
    EnumArray<bool, cross::MouseButton> mouse_buttons_pressed = {};

    Option<int2> scroll_this_frame = {};
    Option<int2> mouse_drag_start  = {};
    Option<int2> mouse_drag_delta  = {};
    Option<int2> mouse_delta       = {};
    int2         mouse_position    = {0};

    friend UI::Context;
};
