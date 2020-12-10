#pragma once
#include "base/types.hpp"

#include <imgui/imgui.h>

namespace my_app
{
struct TransformComponent
{
    float3 position = float3(0.0);
    float3 front    = float3_FORWARD;
    float3 up       = float3_UP;

    static const char *type_name() { return "TransformComponent"; }

    inline void display_ui()
    {
        ImGui::SliderFloat3("Position", position.data(), 0.0f, 1000.f);
    }
};
} // namespace my_app
