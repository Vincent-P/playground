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

namespace my_app
{
    constexpr int FPS_CAP = 200;
    constexpr double MOUSE_SENSITIVITY = 0.3;
    constexpr double CAMERA_SPEED = 0.02;

    class App
    {
        public:
        App()
            : window(create_glfw_window())
            , renderer(window)
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

            if (glfwGetKey(app->window, GLFW_KEY_LEFT_ALT) || !app->is_focused)
                return;

            auto& yaw = app->camera.yaw;
            auto& pitch = app->camera.pitch;

            yaw += static_cast<float>((xpos - last_x) * MOUSE_SENSITIVITY);
            pitch += static_cast<float>((last_y - ypos) * MOUSE_SENSITIVITY);

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

        void update_input(double delta_t)
        {
            if (!is_focused)
                return;

            auto cursor_mode = glfwGetInputMode(window, GLFW_CURSOR);

            if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) && cursor_mode != GLFW_CURSOR_NORMAL)
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            else if (!glfwGetKey(window, GLFW_KEY_LEFT_ALT) && cursor_mode != GLFW_CURSOR_DISABLED)
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

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
                camera.position += float(CAMERA_SPEED * forward * delta_t) * camera.front;

            if (right)
                camera.position += float(CAMERA_SPEED * right * delta_t) * glm::normalize(glm::cross(camera.front, camera.up));
        }

        GLFWwindow* create_glfw_window()
        {
            glfwInit();

            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

            return glfwCreateWindow(WIDTH, HEIGHT, "Test vulkan", nullptr, nullptr);
        }

        void run()
        {
            using clock = std::chrono::steady_clock;

            auto next_frame = clock::now();
            auto start = next_frame;
            auto end = next_frame;
            auto last_fps_update = next_frame;

            uint64_t frame_counter = 0;
            double timer = 0.0;
            double delta_t = 0.0;

            while (!glfwWindowShouldClose(window))
            {
                int visible = glfwGetWindowAttrib(window, GLFW_VISIBLE);
                int focused = glfwGetWindowAttrib(window, GLFW_FOCUSED);
                is_focused = visible && focused;
                if (!is_focused)
                    glfwWaitEvents();

                start = clock::now();
                next_frame += std::chrono::milliseconds(1000 / FPS_CAP);

                glfwPollEvents();
                update_input(delta_t);
                renderer.draw_frame(timer / 1000, camera);

                if (clock::now() < next_frame)
                    std::this_thread::sleep_until(next_frame);

                frame_counter++;

                end = clock::now();

                delta_t = std::chrono::duration<double, std::milli>(end - start).count();
                timer += delta_t;

                if (last_fps_update + std::chrono::milliseconds(1000) < end)
                {
                    std::string windowTitle = "Test vulkan - " + std::to_string(frame_counter) + " fps";
                    glfwSetWindowTitle(window, windowTitle.c_str());
                    frame_counter = 0;
                    last_fps_update = end;
                }
            }

            renderer.wait_idle();
        }

        GLFWwindow* window;
        Renderer renderer;
        Camera camera;
        bool is_focused;
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
