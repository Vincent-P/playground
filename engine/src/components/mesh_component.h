#pragma once
#include <exo/handle.h>

#include <imgui/imgui.h>

struct Mesh;
struct Material;

struct RenderMeshComponent
{
    u32 i_mesh;
    u32 i_material;

    static const char *type_name() { return "RenderMeshComponent"; }

    inline void display_ui()
    {
        ImGui::Text("Mesh index: %u", i_mesh);
        ImGui::Text("Material index: %u", i_material);
    }
};
