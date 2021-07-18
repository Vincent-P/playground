#pragma once
#include <exo/types.h>

#include <imgui/imgui.h>

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

struct LocalToWorldComponent
{
    float4x4 transform;

    static const char *type_name() { return "LocalToWorldComponent"; }

    inline void display_ui()
    {
    }
};
