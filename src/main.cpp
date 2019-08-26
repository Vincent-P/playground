#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include <GLFW/glfw3.h>
#include <chrono>
#include <cstdint>
#include <glm/glm.hpp>
#include <iostream>
#include <string>
#include <thread>
#pragma clang diagnostic pop

#include "renderer.hpp"
#include "timer.h"

namespace my_app
{
    constexpr float MOUSE_SENSITIVITY = 0.3f;
    constexpr float CAMERA_SPEED = 50.f;

    class App
    {
        public:
        App()
            : window(create_glfw_window())
            , renderer(window)
            , camera()
            , is_focused()
            , stop(false)
            , timer()
        {
            glfwSetWindowUserPointer(window, this);

            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            glfwSetCursorPosCallback(window, cursor_callback);
            glfwSetFramebufferSizeCallback(window, resize_callback);
        }

        ~App()
        {
            glfwTerminate();
        }

        GLFWwindow* create_glfw_window()
        {
            glfwInit();

            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

            return glfwCreateWindow(WIDTH, HEIGHT, "Test vulkan", nullptr, nullptr);
        }

        static void resize_callback(GLFWwindow* window, int width, int height)
        {
            if (!width || !height)
                return;

            auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
            app->renderer.resize(width, height);
        }

        static void cursor_callback(GLFWwindow* window, double xpos, double ypos)
        {
            static double last_x = xpos;
            static double last_y = ypos;

            auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));

            if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_NORMAL || !app->is_focused)
                return;

            if (abs(last_x - xpos) > 100 || abs(last_y - ypos) > 100)
            {
                last_x = xpos;
                last_y = ypos;
                return;
            }

            auto& yaw = app->camera.yaw;
            auto& pitch = app->camera.pitch;

            yaw += float(xpos - last_x) * MOUSE_SENSITIVITY;
            pitch += float(last_y - ypos) * MOUSE_SENSITIVITY;

            if (pitch > 89.0f)
                pitch = 89.0f;
            if (pitch < -89.0f)
                pitch = -89.0f;

            glm::vec3 front;
            front.x = cos(glm::radians(pitch)) * cos(glm::radians(yaw));
            front.y = sin(glm::radians(pitch));
            front.z = cos(glm::radians(pitch)) * sin(glm::radians(yaw));
            app->camera.front = glm::normalize(front);

            last_x = xpos;
            last_y = ypos;
        }

        void update_input(float delta_t)
        {
            if (!is_focused)
                return;

            if (glfwGetKey(window, GLFW_KEY_ESCAPE))
            {
                stop = true;
                return;
            }

            auto cursor_mode = glfwGetInputMode(window, GLFW_CURSOR);


            if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) && cursor_mode != GLFW_CURSOR_NORMAL)
            {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                return;
            }
            else if (!glfwGetKey(window, GLFW_KEY_LEFT_ALT) && cursor_mode != GLFW_CURSOR_DISABLED)
            {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                if (glfwRawMouseMotionSupported())
                    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            }

            int forward = 0;
            int right = 0;

            if (glfwGetKey(window, GLFW_KEY_W))
                forward++;
            if (glfwGetKey(window, GLFW_KEY_A))
                right--;
            if (glfwGetKey(window, GLFW_KEY_S))
                forward--;
            if (glfwGetKey(window, GLFW_KEY_D))
                right++;

            if (forward)
                camera.position += (CAMERA_SPEED * float(forward) * delta_t) * camera.front;

            if (right)
                camera.position += (CAMERA_SPEED * float(right) * delta_t) * glm::normalize(glm::cross(camera.front, camera.up));
        }

        void run()
        {
            while (not glfwWindowShouldClose(window) and not stop)
            {
                int visible = glfwGetWindowAttrib(window, GLFW_VISIBLE);
                int focused = glfwGetWindowAttrib(window, GLFW_FOCUSED);
                is_focused = visible && focused;
                if (!is_focused)
                    glfwWaitEvents();

                timer.update();

                glfwPollEvents();
                update_input(timer.get_delta_time());
                renderer.draw_frame(camera);

                std::string windowTitle = "Test vulkan - " + std::to_string(timer.get_average_fps() / 1) + " FPS | " + std::to_string(timer.get_delta_time() * 1000.f) + " ms";
                glfwSetWindowTitle(window, windowTitle.c_str());
            }

            renderer.wait_idle();
        }

        GLFWwindow* window;
        Renderer renderer;
        Camera camera;
        bool is_focused;
        bool stop;
        TimerData timer;
    };
}    // namespace my_app

int main()
{
    try
    {
        my_app::App app;
        app.run();
    }
    catch (std::exception const& e)
    {
        std::cerr << "Exception :" << e.what() << std::endl;
        return 1;
    }
    return 0;
}
