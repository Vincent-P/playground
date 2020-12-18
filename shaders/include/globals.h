#ifndef GLOBALS_H
#define GLOBALS_H

#include "types.h"

layout (set = 0, binding = 0) uniform GlobalUniform {
    float4x4 camera_view;
    float4x4 camera_proj;
    float4x4 camera_inv_proj;
    float4x4 camera_inv_view_proj;
    float4x4 sun_view;
    float4x4 sun_proj;

    float3 camera_pos;
    float delta_t;

    uint2 resolution;
    float camera_near;
    float camera_far;


    float3 sun_direction;
    float pad10;

    float3 sun_illuminance;
    float ambient;
} global;

float reverse_to_linear_depth(float depth)
{
    float z_e = global.camera_proj[3].z / (depth + global.camera_proj[2].z);
    float linear = (z_e - global.camera_near) / (global.camera_far - global.camera_near);
    return linear;
}

float linear_to_reverse_depth(float linear)
{
    float z_e = linear * (global.camera_far - global.camera_near) + global.camera_near;
    float z_n = - global.camera_proj[2].z  - global.camera_proj[3].z / -z_e;
    return z_n;
}

#endif
