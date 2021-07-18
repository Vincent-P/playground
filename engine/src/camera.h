#pragma once

#include <exo/types.h>

namespace camera
{
float4x4 look_at(float3 eye, float3 at, float3 up, float4x4 *inverse = nullptr);
float4x4 perspective(float fov, float aspect_ratio, float near_plane, float far_plane, float4x4 *inverse = nullptr);
float4x4 infinite_perspective(float fov, float aspect_ratio, float near_plane, float4x4 *inverse = nullptr);
float4x4 ortho(float3 min_clip, float3 max_clip, float4x4 *inverse = nullptr);
}
