#include "window.hpp"
#include <GLFW/glfw3.h>

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

    void Window::glfw_click_callback(GLFWwindow* /*window*/, int /*button*/, int /*action*/, int /*thing*/)
    {
	#if 0
	Window* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
	#endif
    }

    bool Window::should_close()
    {
	return glfwWindowShouldClose(window) != 0;
    }

    void Window::update()
    {
        glfwPollEvents();
    }
}    // namespace my_app
