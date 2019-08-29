#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include <GLFW/glfw3.h>
#include <chrono>
#include <cstdint>
#include <glm/glm.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <imgui.h>
#include <math.h>
#pragma clang diagnostic pop

#include "renderer.hpp"
#include "timer.hpp"
#include "tools.hpp"

#define ARRAY_SIZE(_arr) (sizeof(_arr)/sizeof(*_arr))

namespace my_app
{
    constexpr float MOUSE_SENSITIVITY = 0.3f;
    constexpr float CAMERA_SPEED = 50.f;

    class App
    {
        public:
        App(std::string model_path)
            : window(create_glfw_window())
            , renderer(window, model_path)
            , camera()
            , timer()
            , is_focused(true)
            , is_ui(false)
            , stop(false)
        {
            glfwSetWindowUserPointer(window, this);

            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            glfwSetFramebufferSizeCallback(window, resize_callback);
            glfwSetMouseButtonCallback(window, click_callback);
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


        static void click_callback(GLFWwindow* window, int button, int action, int)
        {
            auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
            if (action == GLFW_PRESS && button >= 0 && button < int(ARRAY_SIZE(mouse_just_pressed)))
                app->mouse_just_pressed[button] = true;
        }

        void update_mouse()
        {
            ImGuiIO& io = ImGui::GetIO();


            ImGui::SetNextWindowPos(ImVec2(20.f, 20.0f));
            ImGui::Begin("Mouse Internals", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

            // UI Mode cursor
            if (is_ui)
            {
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
                }


                // Update the mouse buttons
                for (int i = 0; i < int(ARRAY_SIZE(io.MouseDown)); i++)
                {
                    // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
                    io.MouseDown[i] = mouse_just_pressed[i] || glfwGetMouseButton(window, i) != 0;
                    mouse_just_pressed[i] = false;
                }


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
            }
            // Game mode
            else
            {
                double mouse_x, mouse_y;
                glfwGetCursorPos(window, &mouse_x, &mouse_y);

                if (last_xpos == 0.f && last_ypos == 0.f)
                {
                    last_xpos = float(mouse_x);
                    last_ypos = float(mouse_y);
                }

                float yaw_increment = (float(mouse_x) - last_xpos) * MOUSE_SENSITIVITY;
                float pitch_increment = (last_ypos - float(mouse_y)) * MOUSE_SENSITIVITY;

                camera.yaw += yaw_increment;
                camera.pitch += pitch_increment;

                if (isnan(camera.yaw))
                    camera.yaw = 0.f;

                ImGui::SetCursorPosX(10.0f);
                ImGui::Text("GUI Mouse position:");
                ImGui::SetCursorPosX(20.0f);
                ImGui::Text("X: %.1f", double(io.MousePos.x));
                ImGui::SetCursorPosX(20.0f);
                ImGui::Text("Y: %.1f", double(io.MousePos.y));

                ImGui::SetCursorPosX(10.0f);
                ImGui::Text("Mouse position:");
                ImGui::SetCursorPosX(20.0f);
                ImGui::Text("X: %.1f", double(mouse_x));
                ImGui::SetCursorPosX(20.0f);
                ImGui::Text("Y: %.1f", double(mouse_y));

                ImGui::SetCursorPosX(10.0f);
                ImGui::Text("Last Mouse position:");
                ImGui::SetCursorPosX(20.0f);
                ImGui::Text("X: %.1f", double(last_xpos));
                ImGui::SetCursorPosX(20.0f);
                ImGui::Text("Y: %.1f", double(last_ypos));

                ImGui::SetCursorPosX(10.0f);
                ImGui::Text("Camera:");
                ImGui::SetCursorPosX(20.0f);
                ImGui::Text("Pitch: %.1f", double(camera.pitch));
                ImGui::SetCursorPosX(20.0f);
                ImGui::Text("Yaw: %.1f", double(camera.yaw));
                ImGui::SetCursorPosX(20.0f);
                ImGui::Text("Pitch increment: %.1f", double(yaw_increment));
                ImGui::SetCursorPosX(20.0f);
                ImGui::Text("Yaw increment: %.1f", double(pitch_increment));


                if (camera.pitch > 89.0f)
                    camera.pitch = 89.0f;
                if (camera.pitch < -89.0f)
                    camera.pitch = -89.0f;

                while (camera.yaw > 180.0f)
                    camera.yaw -= 360.0f;

                while (camera.yaw < -180.0f)
                    camera.yaw += 360.0f;

                glm::vec3 front;
                front.x = cos(glm::radians(camera.pitch)) * cos(glm::radians(camera.yaw));
                front.y = sin(glm::radians(camera.pitch));
                front.z = cos(glm::radians(camera.pitch)) * sin(glm::radians(camera.yaw));
                camera.front = glm::normalize(front);

                last_xpos = float(mouse_x);
                last_ypos = float(mouse_y);
            }

            ImGui::End();
        }

        void update_keyboard(float delta_t)
        {
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

        void update_input(float delta_t)
        {
            is_focused = glfwGetWindowAttrib(window, GLFW_VISIBLE) && glfwGetWindowAttrib(window, GLFW_FOCUSED);

            if (!is_focused)
                glfwWaitEvents();

            // Quit when escape is pressed
            if (glfwGetKey(window, GLFW_KEY_ESCAPE))
            {
                stop = true;
                return;
            }

            // Update cursor with ALT
            auto cursor_mode = glfwGetInputMode(window, GLFW_CURSOR);

            // Free the cursor to with ALT
            if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) && cursor_mode != GLFW_CURSOR_NORMAL)
            {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

                // I need this to prevent cursor jumping for some reasons
                double mouse_x, mouse_y;
                glfwGetCursorPos(window, &mouse_x, &mouse_y);
                last_xpos = float(mouse_x);
                last_ypos = float(mouse_y);
            }
            // If ALT isn't pressed the mouse input is raw
            else if (!glfwGetKey(window, GLFW_KEY_LEFT_ALT) && cursor_mode != GLFW_CURSOR_DISABLED)
            {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                if (glfwRawMouseMotionSupported())
                    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

                // I need this to prevent cursor jumping for some reasons
                double mouse_x, mouse_y;
                glfwGetCursorPos(window, &mouse_x, &mouse_y);
                last_xpos = float(mouse_x);
                last_ypos = float(mouse_y);
            }

            is_ui = cursor_mode == GLFW_CURSOR_NORMAL;

            update_mouse();
            update_keyboard(delta_t);
        }

        void run()
        {

            while (not glfwWindowShouldClose(window) and not stop)
            {
                glfwPollEvents();

                ImGui::NewFrame();

                timer.update();
                update_input(timer.get_delta_time());

                renderer.draw_frame(camera, timer);
            }

            renderer.wait_idle();
        }

        GLFWwindow* window;
        Renderer renderer;
        Camera camera;
        TimerData timer;
        bool is_focused;
        bool is_ui;
        bool stop;
        bool mouse_just_pressed[5] = { false, false, false, false, false };
        float last_xpos;
        float last_ypos;
    };
}    // namespace my_app

int main(int, char* argv[])
{
    try
    {
        std::string model = "models/Sponza/glTF/Sponza.gltf";
        if (argv[1])
            model = argv[1];

        my_app::App app(model);
        app.run();
    }
    catch (std::exception const& e)
    {
        std::cerr << "Exception :" << e.what() << std::endl;
        return 1;
    }
    return 0;
}
