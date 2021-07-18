#pragma once
#include <exo/types.h>

#include <imgui/imgui.h>

struct CameraComponent
{
    float near_plane = 0.1f;
    float far_plane  = 100000.0f;
    float fov        = 90.0f;
    float4x4 view;
    float4x4 view_inverse;
    float4x4 projection;
    float4x4 projection_inverse;

    static const char *type_name() { return "CameraComponent"; }

    inline void display_ui()
    {
        ImGui::SliderFloat("Near plane", &near_plane, 0.1f, 1.f);
        ImGui::SliderFloat("Far plane", &far_plane, 100.0f, 1000.f);
        ImGui::SliderFloat("FOV", &fov, 45.f, 90.f);
    }
};
