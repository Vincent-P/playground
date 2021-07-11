#include "asset_manager.hpp"

#include "imgui/imgui.h"
#include "platform/file_dialog.hpp"
#include "ui.hpp"

#include <fmt/format.h>

void AssetManager::load_texture(const std::filesystem::path &path)
{
    textures.add({.name = path.string()});
}

void AssetManager::choose_texture(Handle<Texture> &selected)
{
    if (selected.is_valid())
    {
        ImGui::Text("Selected #%u", selected.value());
    }
    else
    {
        ImGui::Text("<None>");
    }

    {
        int frame_padding = -1;                             // -1 == uses default padding (style.FramePadding)
        ImVec2 size       = ImVec2(32.0f, 32.0f);           // Size of the image we want to make visible
        ImVec2 uv0        = ImVec2(0.0f, 0.0f);             // UV coordinates for lower-left
        ImVec2 uv1        = ImVec2(1.0f, 1.0f);             // UV coordinates for (32,32) in our texture
        ImVec4 bg_col     = ImVec4(0.0f, 0.0f, 0.0f, 1.0f); // Black background
        ImVec4 tint_col   = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // No tint
        if (ImGui::ImageButton(0, size, uv0, uv1, frame_padding, bg_col, tint_col))
        {
            ImGui::OpenPopup("textureselect");
        }
    }

    if (ImGui::BeginPopup("textureselect"))
    {
        ImGui::Text("Textures (%u):", textures.size());
        ImGui::Separator();

        if (ImGui::BeginTable("Assets", 4))
        {
            ImGui::TableSetupColumn("Handle");
            ImGui::TableSetupColumn("Textures");
            ImGui::TableSetupColumn("Path");
            ImGui::TableSetupColumn("");

            ImGui::TableHeadersRow();

            for (auto [h, p_texture] : textures)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::Text("#%u", h.value());

                ImGui::TableNextColumn();

                ImGui::Image(0, ImVec2(32, 32));

                ImGui::TableNextColumn();

                auto label   = fmt::format("{}", p_texture->name);
                bool clicked = ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
                ImGui::TableNextColumn();

                if (clicked)
                {
                    selected = h;
                }
            }

            ImGui::EndTable();
        }

        ImGui::EndPopup();
    }
}

void AssetManager::choose_mesh(Handle<Mesh> &selected)
{
    if (selected.is_valid())
    {
        ImGui::Text("Selected #%u", selected.value());
    }
    else
    {
        ImGui::Text("<None>");
    }

    {
        int frame_padding = -1;                             // -1 == uses default padding (style.FramePadding)
        ImVec2 size       = ImVec2(32.0f, 32.0f);           // Size of the image we want to make visible
        ImVec2 uv0        = ImVec2(0.0f, 0.0f);             // UV coordinates for lower-left
        ImVec2 uv1        = ImVec2(1.0f, 1.0f);             // UV coordinates for (32,32) in our texture
        ImVec4 bg_col     = ImVec4(0.0f, 0.0f, 0.0f, 1.0f); // Black background
        ImVec4 tint_col   = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // No tint
        if (ImGui::ImageButton(0, size, uv0, uv1, frame_padding, bg_col, tint_col))
        {
            ImGui::OpenPopup("meshselect");
        }
    }
}


void AssetManager::display_ui(UI::Context &ui)
{
    if (ui.begin_window("Assets"))
    {
        if (ImGui::Button("Load texture"))
        {
            auto file_path = platform::file_dialog({{"PNG", "*.png"}, {"JPG", "*.jpg"}});
            if (file_path)
            {
                load_texture(*file_path);
            }
        }

        static Handle<Texture> s_texture_example {};
        choose_texture(s_texture_example);

        ui.end_window();
    }
}
