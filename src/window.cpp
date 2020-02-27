#include "window.hpp"
#include "types.hpp"
#include <GLFW/glfw3.h>
#include <imgui.h>

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

    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    glfwGetMonitorContentScale(primary, &dpi_scale.x, &dpi_scale.y);
}

Window::~Window() { glfwTerminate(); }

void Window::glfw_resize_callback(GLFWwindow *window, int width, int height)
{
    auto *self = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
    self->resize_callback(width, height);
}

void Window::resize_callback(int width, int height)
{
    for (const auto &cb : resize_callbacks) {
        cb(width, height);
    }
}

void Window::register_resize_callback(const std::function<void(int, int)> &callback)
{
    resize_callbacks.push_back(callback);
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

bool Window::should_close() { return glfwWindowShouldClose(window) != 0; }

void Window::update()
{
    glfwPollEvents();

    ImGuiIO &io = ImGui::GetIO();

    // Update the mouse position for ImGui
    if (io.WantSetMousePos) {
        glfwSetCursorPos(window, double(io.MousePos.x), double(io.MousePos.y));
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

    static bool init = true;
    if (init) {
        ImGui::SetNextWindowPos(ImVec2(10.f * dpi_scale.x, 10.0f * dpi_scale.y));
        ImGui::Begin("Mouse Internals", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    }
    else {
        ImGui::Begin("Mouse Internals", nullptr, ImGuiWindowFlags_NoScrollbar);
    }
    init = false;

    ImGui::Text("DisplaySize = %f,%f", static_cast<double>(io.DisplaySize.x), static_cast<double>(io.DisplaySize.y));

    ImGui::Text("Dpi scale = %f,%f", static_cast<double>(dpi_scale.x), static_cast<double>(dpi_scale.y));

    ImGui::SetCursorPosX(10.0f);
    ImGui::Text("GUI Mouse position:");
    ImGui::SetCursorPosX(20.0f);
    ImGui::Text("X: %.1f", double(io.MousePos.x));
    ImGui::SetCursorPosX(20.0f);
    ImGui::Text("Y: %.1f", double(io.MousePos.y));

    ImGui::SetCursorPosX(10.0f);
    ImGui::Text("Last Mouse position:");
    ImGui::SetCursorPosX(20.0f);
    ImGui::Text("X: %.1f", double(last_xpos));
    ImGui::SetCursorPosX(20.0f);
    ImGui::Text("Y: %.1f", double(last_ypos));
    ImGui::End();
}

} // namespace my_app
