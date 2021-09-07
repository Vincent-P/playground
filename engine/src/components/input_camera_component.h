#pragma once
#include <exo/types.h>
#include <exo/collections/enum_array.h>

#include <array>
#include <imgui/imgui.h>

struct InputCameraComponent
{
    enum struct States : uint
    {
        Idle,
        Move,
        Orbit,
        Zoom,
        Count
    };

    States state = States::Idle;
    // spherical coordinates: radius r, azymuthal angle theta, polar angle phi
    float              r      = 6.0f;
    float              theta  = -78.0f;
    float              phi    = -65.0f;
    float3             target = {0.0f};

    static const char *type_name() { return "InputCameraComponent"; }
    inline void display_ui();
};

static constexpr EnumArray<const char *, InputCameraComponent::States> input_camera_states_to_string_array{
    "Idle",
    "Move",
    "Orbit",
    "Zoom",
};

inline constexpr const char *to_string(InputCameraComponent::States action)
{
    return input_camera_states_to_string_array[action];
}

inline void InputCameraComponent::display_ui()
{
    const char *state_str = to_string(state);
    ImGui::Text("State: %s", state_str);
}
