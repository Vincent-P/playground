#pragma once
#include "base/types.hpp"
#include "eva-icons.hpp"

#include <imgui/imgui.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace platform { struct Window; }

class Inputs;
namespace UI
{

struct Window
{
    std::string name                = "no-name";
    bool is_visible                 = true;
};

struct Context
{
    static void create(Context &ctx);
    void destroy();

    void start_frame(platform::Window &window, Inputs &inputs);
    void display_ui();
    void on_mouse_movement(platform::Window &window, double xpos, double ypos);

    bool begin_window(std::string_view name, bool is_visible = true, ImGuiWindowFlags flags = 0);
    void end_window();

    std::unordered_map<std::string_view, Window> windows;
};
} // namespace UI
