#include <GLFW/glfw3.h>

#include <chrono>
#include <cstdint>
#include <glm/glm.hpp>
#include <iostream>
#include <string>
#include <thread>

#include "renderer.hpp"

namespace my_app
{
    constexpr int fps_cap = 200;
    constexpr double mouse_sensitivity = 0.3;
    constexpr double camera_speed = 0.02;

    class App
    {
        public:
        App()
            : window_(CreateGLFWWindow())
            , renderer_(window_)
        {
            glfwSetWindowUserPointer(window_, this);

            glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            glfwSetCursorPosCallback(window_, CursorCallback);
            glfwSetFramebufferSizeCallback(window_, ResizeCallback);
        }

        ~App()
        {
            glfwTerminate();
        }

        static void ResizeCallback(GLFWwindow* window, int width, int height)
        {
            if (!width || !height)
                return;

            auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
            app->renderer_.Resize(width, height);
        }

        static void CursorCallback(GLFWwindow* window, double xpos, double ypos)
        {
            static double last_x = xpos;
            static double last_y = ypos;

            auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));

            if (glfwGetKey(app->window_, GLFW_KEY_LEFT_ALT) || !app->is_focused_)
                return;

            auto& yaw = app->camera_.yaw;
            auto& pitch = app->camera_.pitch;

            yaw += (xpos - last_x) * mouse_sensitivity;
            pitch += (last_y - ypos) * mouse_sensitivity;

            if (pitch > 89.0f)
                pitch = 89.0f;
            if (pitch < -89.0f)
                pitch = -89.0f;

            glm::vec3 front;
            front.x = cos(glm::radians(pitch)) * cos(glm::radians(yaw));
            front.y = sin(glm::radians(pitch));
            front.z = cos(glm::radians(pitch)) * sin(glm::radians(yaw));
            app->camera_.front = glm::normalize(front);

            last_x = xpos;
            last_y = ypos;
        }

        void UpdateInput(double delta_t)
        {
            if (!is_focused_)
                return;

            auto cursor_mode = glfwGetInputMode(window_, GLFW_CURSOR);

            if (glfwGetKey(window_, GLFW_KEY_LEFT_ALT) && cursor_mode != GLFW_CURSOR_NORMAL)
                glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            else if (!glfwGetKey(window_, GLFW_KEY_LEFT_ALT) && cursor_mode != GLFW_CURSOR_DISABLED)
                glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

            int forward = 0;
            int right = 0;

            if (glfwGetKey(window_, GLFW_KEY_W))
                forward++;
            if (glfwGetKey(window_, GLFW_KEY_A))
                right--;
            if (glfwGetKey(window_, GLFW_KEY_S))
                forward--;
            if (glfwGetKey(window_, GLFW_KEY_D))
                right++;

            if (forward)
                camera_.position += float(camera_speed * forward * delta_t) * camera_.front;

            if (right)
                camera_.position += float(camera_speed * right * delta_t) * glm::normalize(glm::cross(camera_.front, camera_.up));
        }

        GLFWwindow* CreateGLFWWindow()
        {
            glfwInit();

            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

            return glfwCreateWindow(WIDTH, HEIGHT, "Test vulkan", nullptr, nullptr);
        }

        void Run()
        {
            using clock = std::chrono::steady_clock;

            auto next_frame = clock::now();
            auto start = next_frame;
            auto end = next_frame;
            auto last_fps_update = next_frame;

            uint64_t frame_counter = 0;
            double timer = 0.0;
            double delta_t = 0.0;

            while (!glfwWindowShouldClose(window_))
            {
                int visible = glfwGetWindowAttrib(window_, GLFW_VISIBLE);
                int focused = glfwGetWindowAttrib(window_, GLFW_FOCUSED);
                is_focused_ = visible && focused;
                if (!is_focused_)
                    glfwWaitEvents();

                start = clock::now();
                next_frame += std::chrono::milliseconds(1000 / fps_cap);

                glfwPollEvents();
                UpdateInput(delta_t);
                renderer_.DrawFrame(timer / 1000, camera_);

                if (clock::now() < next_frame)
                    std::this_thread::sleep_until(next_frame);

                frame_counter++;

                end = clock::now();

                delta_t = std::chrono::duration<double, std::milli>(end - start).count();
                timer += delta_t;

                if (last_fps_update + std::chrono::milliseconds(1000) < end)
                {
                    std::string windowTitle = "Test vulkan - " + std::to_string(frame_counter) + " fps";
                    glfwSetWindowTitle(window_, windowTitle.c_str());
                    frame_counter = 0;
                    last_fps_update = end;
                }
            }

            renderer_.WaitIdle();
        }

        GLFWwindow* window_;
        Renderer renderer_;
        Camera camera_;
        bool is_focused_;
    };
}    // namespace my_app

int main()
{
    try
    {
        my_app::App app;
        app.Run();
    }
    catch (std::exception const& e)
    {
        std::cerr << "Exception :" << e.what() << std::endl;
        return 1;
    }
    return 0;
}
