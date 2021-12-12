#include "gameplay/components/mesh_component.h"
#include <imgui.h>

void MeshComponent::show_inspector_ui()
{
    SpatialComponent::show_inspector_ui();

    ImGui::Text("Mesh: %s", mesh_asset.str);
}
