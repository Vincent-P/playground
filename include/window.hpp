#pragma once
#include <GLFW/glfw3.h>
#include "types.hpp"
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
	GLFWwindow* get_handle() const { return window; }

	bool should_close();
	void update();

	void register_resize_callback(const std::function<void(int, int)>& callback);

	private:
	static void glfw_resize_callback(GLFWwindow* window, int width, int height);
	static void glfw_click_callback(GLFWwindow* window, int button, int action, int thing);

	void resize_callback(int width, int height);

	std::vector<std::function<void(int, int)>> resize_callbacks;

        GLFWwindow* window;
    };
}    // namespace my_app
