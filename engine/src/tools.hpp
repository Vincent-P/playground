#pragma once

#include "base/types.hpp"
#include "base/vector.hpp"

#include <filesystem>
#include <imgui/imgui.h>
#include <string>

namespace fs = std::filesystem;

namespace tools
{
struct MouseState
{
    bool left_pressed;
    bool right_pressed;
    double xpos;
    double ypos;
};

Vec<u8> read_file(const fs::path &path);

inline void imgui_select(const char *title, const char **items, usize items_size, uint &current_item)
{
    std::string id("##custom combo");
    id += title;

    ImGui::Text("%s", title);
    if (ImGui::BeginCombo(id.c_str(), items[current_item], ImGuiComboFlags_NoArrowButton)) {
	for (uint n = 0; n < items_size; n++) {
	    bool is_selected = (current_item == n);
	    if (ImGui::Selectable(items[n], is_selected)) {
		current_item = n;
	    }
	    if (is_selected) {
		ImGui::SetItemDefaultFocus();
	    }
        }
        ImGui::EndCombo();
    }
}
}
