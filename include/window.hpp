#pragma once
#include "types.hpp"
#include <GLFW/glfw3.h>
#include <array>
#include <functional>
#include <vector>

namespace my_app
{

class Window
{
  public:
    explicit Window(int width, int height);
    ~Window();

    NO_COPY_NO_MOVE(Window)

    void run();
    [[nodiscard]] GLFWwindow *get_handle() const { return window; }
    [[nodiscard]] float2 get_dpi_scale() const { return dpi_scale; }

    bool should_close();
    void update();

    void register_resize_callback(const std::function<void(int, int)> &callback);
    void register_mouse_callback(const std::function<void(double, double)> &callback);
    void register_scroll_callback(const std::function<void(double, double)> &callback);

  private:
    static void glfw_resize_callback(GLFWwindow *window, int width, int height);
    static void glfw_click_callback(GLFWwindow *window, int button, int action, int thing);
    static void glfw_cursor_position_callback(GLFWwindow *window, double xpos, double ypos);
    static void glfw_scroll_callback(GLFWwindow *window, double xoffset, double yoffset);

    std::vector<std::function<void(int, int)>> resize_callbacks;
    std::vector<std::function<void(double, double)>> mouse_callbacks;
    std::vector<std::function<void(double, double)>> scroll_callbacks;

    bool force_close{false};
    GLFWwindow *window;
    double last_xpos, last_ypos;
    float2 dpi_scale;
    std::array<bool, 5> mouse_just_pressed = {false, false, false, false, false};
};

} // namespace my_app
