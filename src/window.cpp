#include "window.hpp"
#include "types.hpp"
#include <GLFW/glfw3.h>
#include <iostream>
#if defined(ENABLE_IMGUI)
#include <imgui.h>
#endif

namespace my_app
{

Window::Window(int width, int height)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    this->window = glfwCreateWindow(width, height, "Test vulkan", nullptr, nullptr);

    glfwSetWindowUserPointer(this->window, this);

    glfwSetFramebufferSizeCallback(this->window, glfw_resize_callback);
    glfwSetMouseButtonCallback(this->window, glfw_click_callback);
    glfwSetCursorPosCallback(this->window, glfw_cursor_position_callback);

    GLFWmonitor *primary = glfwGetPrimaryMonitor();
    glfwGetMonitorContentScale(primary, &dpi_scale.x, &dpi_scale.y);
}

Window::~Window() { glfwTerminate(); }

void Window::register_resize_callback(const std::function<void(int, int)> &callback)
{
    resize_callbacks.push_back(callback);
}

void Window::register_mouse_callback(const std::function<void(double, double)> &callback)
{
    mouse_callbacks.push_back(callback);
}

void Window::glfw_resize_callback(GLFWwindow *window, int width, int height)
{
    auto *self = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
    for (const auto &cb : self->resize_callbacks) {
	cb(width, height);
    }
}

void Window::glfw_click_callback(GLFWwindow *window, int button, int action, int /*thing*/)
{
    auto *self = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
    if (action == GLFW_PRESS && button >= 0) {
	auto ibutton = static_cast<usize>(button);
	if (ibutton < self->mouse_just_pressed.size()) {
	    self->mouse_just_pressed[ibutton] = true;
	}
    }
}

void Window::glfw_cursor_position_callback(GLFWwindow *window, double xpos, double ypos)
{
    auto *self = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
    for (const auto &cb : self->mouse_callbacks) {
	cb(xpos, ypos);
    }
}

bool Window::should_close() { return force_close || glfwWindowShouldClose(window) != 0; }

void Window::update()
{
    glfwPollEvents();

    if (glfwGetKey(window, GLFW_KEY_ESCAPE)) {
	force_close = true;
    }

#if defined(ENABLE_IMGUI)
    ImGuiIO &io = ImGui::GetIO();

    // Update the mouse position for ImGui
    if (io.WantSetMousePos) {
	glfwSetCursorPos(this->window, double(io.MousePos.x), double(io.MousePos.y));
    }
    else {
	double mouse_x;
	double mouse_y;
	glfwGetCursorPos(window, &mouse_x, &mouse_y);
	io.MousePos = ImVec2(float(mouse_x) / dpi_scale.x, float(mouse_y) / dpi_scale.y);

	last_xpos = mouse_x;
	last_ypos = mouse_y;
    }

    // Update the mouse buttons
    for (usize i = 0; i < ARRAY_SIZE(io.MouseDown); i++) {
	// If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events
	// that are shorter than 1 frame.
	io.MouseDown[i]       = mouse_just_pressed[i] || glfwGetMouseButton(window, static_cast<int>(i)) != 0;
	mouse_just_pressed[i] = false;
    }
#endif
}

} // namespace my_app
