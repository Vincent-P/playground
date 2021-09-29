#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

using ImGuiWindowFlags = int;
namespace cross { struct Window; }

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
    static Context create();
    void destroy();

    void start_frame(cross::Window &window, Inputs &inputs);
    void display_ui();
    void on_mouse_movement(cross::Window &window, double xpos, double ypos);

    bool begin_window(std::string_view name, bool is_visible = true, ImGuiWindowFlags flags = 0);
    void end_window();

    std::unordered_map<std::string_view, Window> windows;
};
} // namespace UI
