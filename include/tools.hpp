#pragma once

#include <imgui.h>
#include <iostream>
#include <string>
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

#define ARRAY_SIZE(_arr) (sizeof(_arr) / sizeof(*_arr))

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

    inline void imgui_select(const char* title, const char* items[], size_t items_size, size_t& current_item)
    {
        std::string id("##custom combo");
        id += title;

        ImGui::Text("%s", title);
        if (ImGui::BeginCombo(id.c_str(), items[current_item], ImGuiComboFlags_NoArrowButton))
        {
            for (size_t n = 0; n < items_size; n++)
            {
                bool is_selected = (current_item == n);
                if (ImGui::Selectable(items[n], is_selected))
                    current_item = n;
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }
}    // namespace my_app::tools
