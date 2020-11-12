#pragma once
#include "base/types.hpp"
#include "eva-icons.hpp"

#include <imgui/imgui.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace window
{
struct Window;
};

namespace my_app
{
namespace UI
{
struct Window
{
    std::string name;
    bool is_visible;
};

struct Context
{
    static void create(Context &ctx);
    void destroy();

    void start_frame(::window::Window &window);
    void display_ui();
    void on_mouse_movement(::window::Window &window, double xpos, double ypos);

    bool begin_window(std::string_view name, bool is_visible = false, ImGuiWindowFlags flags = 0);
    void end_window();
    std::unordered_map<std::string_view, Window> windows;
};
} // namespace UI

} // namespace my_app
