#pragma once
#include "base/handle.hpp"

#include <imgui/imgui.h>

struct Mesh;
struct Material;

struct RenderMeshComponent
{
    Handle<Mesh> mesh_handle;
    u32 i_material;

    static const char *type_name() { return "RenderMeshComponent"; }

    inline void display_ui()
    {
        ImGui::Text("Mesh index: %u", mesh_handle.value());
        ImGui::Text("Material index: %u", i_material);
    }
};
