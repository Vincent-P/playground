#ifndef GLOBALS_H
#define GLOBALS_H

#include "types.h"

layout (set = 0, binding = 1) uniform sampler2D global_textures[];
layout (set = 0, binding = 1) uniform sampler3D global_textures_3d[];

#if 0
layout (set = 0, binding = 0) uniform GlobalUniform {
    float4x4 camera_view;
    float4x4 camera_proj;
    float4x4 camera_inv_view;
    float4x4 camera_inv_proj;
    float4x4 camera_inv_view_proj;
    float4x4 camera_previous_view;
    float4x4 camera_previous_proj;
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

    float2 jitter_offset;
    float2 previous_jitter_offset;
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

float4x4 get_jittered_projection(float4x4 projection, float2 jitter_offset)
{
    float4x4 jittered = projection; // global.camera_proj;
    jittered[2][0] = jitter_offset.x;
    jittered[2][1] = jitter_offset.y;
    return jittered;
}

float4x4 get_jittered_inv_projection(float4x4 inv_proj, float2 jitter_offset)
{
    float4x4 jittered = inv_proj; // global.camera_inv_proj;
    jittered[3][0] = jitter_offset.x * inv_proj[0][0];
    jittered[3][1] = jitter_offset.y * inv_proj[1][1];
    return jittered;
}
#endif

#endif
