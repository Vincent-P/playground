#pragma once

#include "base/types.hpp"
#include "platform/window.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace my_app
{

// clang-format off
    namespace UI { struct Context; };
// clang-format on

enum struct Action : uint
{
    QuitApp,
    CameraModifier,
    CameraMove,
    CameraOrbit,
};

struct KeyBinding
{
    // all keys need to be pressed
    std::vector<VirtualKey> keys;
    std::vector<MouseButton> mouse_buttons;
};

class Inputs
{
  public:
    void bind(Action action, const KeyBinding &binding);

    bool is_pressed(Action action);
    bool is_pressed(VirtualKey Key);
    bool is_pressed(MouseButton mouse_button);
    inline std::optional<int2> get_scroll_this_frame() { return scroll_this_frame; }
    inline std::optional<int2> get_mouse_delta() { return mouse_delta; }

    void process(const std::vector<platform::event::Event> &events);

    void display_ui(UI::Context &ui);

  private:
    std::unordered_map<Action, KeyBinding> bindings;

    std::array<bool, to_underlying(VirtualKey::Count) + 1> keys_pressed           = {};
    std::array<bool, to_underlying(MouseButton::Count) + 1> mouse_buttons_pressed = {};

    std::optional<int2> scroll_this_frame = {};
    std::optional<int2> mouse_drag_start  = {};
    std::optional<int2> mouse_drag_delta  = {};
    std::optional<int2> mouse_delta       = {};
    int2 mouse_position = {};
};
} // namespace my_app
