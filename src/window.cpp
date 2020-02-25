#include "window.hpp"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include "types.hpp"

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
    }

    Window::~Window()
    {
	glfwTerminate();
    }

    void Window::glfw_resize_callback(GLFWwindow* window, int width, int height)
    {
	Window* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
	self->resize_callback(width, height);
    }

    void Window::resize_callback(int width, int height)
    {
	for (const auto& cb : resize_callbacks) {
	    cb(width, height);
	}
    }

    void Window::register_resize_callback(const std::function<void(int, int)>& callback)
    {
	resize_callbacks.push_back(callback);
    }

    void Window::glfw_click_callback(GLFWwindow* window, int button, int action, int /*thing*/)
    {
	Window* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
        if (action == GLFW_PRESS && button >= 0 && button < int(ARRAY_SIZE(mouse_just_pressed)))
            self->mouse_just_pressed[button] = true;
    }

    bool Window::should_close()
    {
	return glfwWindowShouldClose(window) != 0;
    }

    void Window::update()
    {
        glfwPollEvents();

        ImGuiIO& io = ImGui::GetIO();

        // Update the mouse position for ImGui
        if (io.WantSetMousePos)
        {
            glfwSetCursorPos(window, double(io.MousePos.x), double(io.MousePos.y));
        }
        else
        {
            double mouse_x, mouse_y;
            glfwGetCursorPos(window, &mouse_x, &mouse_y);
            io.MousePos = ImVec2(float(mouse_x), float(mouse_y));

            last_xpos = float(mouse_x);
            last_ypos = float(mouse_y);
        }

        // Update the mouse buttons
        for (int i = 0; i < int(ARRAY_SIZE(io.MouseDown)); i++)
        {
            // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
            io.MouseDown[i] = mouse_just_pressed[i] || glfwGetMouseButton(window, i) != 0;
            mouse_just_pressed[i] = false;
        }

        ImGui::SetNextWindowPos(ImVec2(20.f, 20.0f));
        ImGui::Begin("Mouse Internals", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

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
}    // namespace my_app
