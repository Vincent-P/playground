#pragma once

#include <string>
#include <iostream>
#include <vector>

#include "timer.hpp"

#define VK_CHECK(x)                                         \
    do                                                      \
    {                                                       \
        VkResult err = x;                                   \
        if (err)                                            \
        {                                                   \
            std::string error("Vulkan error");              \
            error = std::to_string(err) + std::string("."); \
            throw std::runtime_error(error);                \
        }                                                   \
    } while (0)

namespace my_app::tools
{

    struct MouseState
    {
        bool left_pressed;
        bool right_pressed;
        double xpos;
        double ypos;
    };

    std::vector<char> readFile(const std::string& filename);

    inline void start_log(const char* message)
    {
        std::cout << message;
    }

    inline void log(time_t& start_time, const char* message)
    {
        auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(clock_t::now() - start_time);
        std::cout << " (" << milliseconds.count() << "ms)" << "\n"
                  << message;
        start_time = clock_t::now();
    }

    inline void end_log(time_t& start_time, const char* message)
    {
        auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(clock_t::now() - start_time);
        std::cout << " (" << milliseconds.count() << "ms)" << "\n"
                  << message << "\n";
        start_time = clock_t::now();
    }
}    // namespace my_app::tools
