#pragma once
#include <exo/collections/enum_array.h>

enum struct UpdateStage
{
	FrameStart,
	Input,
	PrePhysics,
	Physics,
	PostPhysics,
	FrameEnd,
	Count
};

inline constexpr exo::EnumArray<const char *, UpdateStage> update_stages_to_string_array{
	"FrameStart", "Input", "PrePhysics", "Physics", "PostPhysics", "FrameEnd"};

inline constexpr const char *to_string(UpdateStage stage) { return update_stages_to_string_array[stage]; }
