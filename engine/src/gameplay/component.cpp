#include "gameplay/component.h"
#include <imgui.h>

void SpatialComponent::set_local_transform(const float4x4 &new_transform)
{
    local_transform = new_transform;
    this->update_world_transform();
}

void SpatialComponent::set_local_bounds(const AABB &new_bounds)
{
    local_bounds = new_bounds;
    // TODO: compute world_bounds
}

void SpatialComponent::update_world_transform()
{
    world_transform = local_transform;
    SpatialComponent *p = parent;
    while (p != nullptr)
    {
        world_transform = p->local_transform * world_transform;
        p               = p->parent;
    }

    for (auto *child : children)
    {
        child->update_world_transform();
    }
}

void SpatialComponent::show_inspector_ui()
{
    bool transform_changed = false;
    transform_changed = transform_changed || ImGui::SliderFloat3("LocalTranslation", local_transform.col(3).data(), -100.0f, 100.0f);

    ImGui::Separator();

    ImGui::Text("Local transform");
    if (ImGui::BeginTable("Local transform", 4, ImGuiTableFlags_Borders))
    {
        for (usize i_row = 0; i_row < 4; i_row += 1)
        {
            for (usize i_col = 0; i_col < 4; i_col += 1)
            {
                ImGui::TableNextColumn();
                ImGui::Text("%f", local_transform.at(i_row, i_col));
            }
        }
        ImGui::EndTable();
    }

    if (transform_changed)
    {
        this->update_world_transform();
    }
}
