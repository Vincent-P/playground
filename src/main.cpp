#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

#include "renderer.hpp"

namespace my_app
{
    class Application
    {
        public:
        Application()
            : window_(CreateGLFWWindow())
            , renderer_(window_)
        {}

        ~Application()
        {
            glfwTerminate();
        }

        GLFWwindow* CreateGLFWWindow()
        {
            glfwInit();

            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

            return glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        }

        void run()
        {
            uint64_t frameCounter = 0;
            double frameTimer = 0.0;
            double fpsTimer = 0.0;
            double lastFPS = 0.0;
            double timer = 0.0;

            while (!glfwWindowShouldClose(window_))
            {
                glfwPollEvents();

                auto tStart = std::chrono::high_resolution_clock::now();

                renderer_.DrawFrame(timer);

                frameCounter++;
                auto tEnd = std::chrono::high_resolution_clock::now();
                auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
                frameTimer = tDiff / 1000.0;

                fpsTimer += tDiff;
                timer += tDiff;
                if (fpsTimer > 1000.0)
                {
                    std::string windowTitle = "Test vulkan - " + std::to_string(frameCounter) + " fps"
                        + " - "+ std::to_string(timer);
                    glfwSetWindowTitle(window_, windowTitle.c_str());

                    lastFPS = roundf(1.0 / frameTimer);
                    fpsTimer = 0.0;
                    frameCounter = 0;
                }
            }

            renderer_.WaitIdle();
        }

        private:
        GLFWwindow* window_;
        Renderer renderer_;
    };
}    // namespace my_app

int main()
{
    my_app::Application app;
    app.run();

    return 0;
}
