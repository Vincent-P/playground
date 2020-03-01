#pragma once

#include <imgui.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

#include "types.hpp"

namespace my_app::tools
{
namespace fs = std::filesystem;

struct MouseState
{
    bool left_pressed;
    bool right_pressed;
    double xpos;
    double ypos;
};

std::vector<u8> read_file(const fs::path &path);

inline void imgui_select(const char* title, const char** items, usize items_size, usize& current_item)
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

} // namespace my_app::tools
