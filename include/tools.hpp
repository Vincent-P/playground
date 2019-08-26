#pragma once

#include <string>
#include <vector>

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
}    // namespace my_app::tools
