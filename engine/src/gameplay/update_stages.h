#pragma once
#include <exo/collections/enum_array.h>

enum struct UpdateStages
{
    FrameStart,
    Input,
    PrePhysics,
    Physics,
    PostPhysics,
    FrameEnd,
    Count
};


inline constexpr EnumArray<const char *, UpdateStages> update_stages_to_string_array{
    "FrameStart",
    "Input",
    "PrePhysics",
    "Physics",
    "PostPhysics",
    "FrameEnd"
};

inline constexpr const char *to_string(UpdateStages stage) { return update_stages_to_string_array[stage]; }
