#include "types.h"

layout (set = 0, binding = 0) uniform GlobalUniform {
    float4x4 camera_view;
    float4x4 camera_proj;
    float4x4 camera_inv_proj;
    float4x4 camera_inv_view_proj;
    float4x4 sun_view;
    float4x4 sun_proj;

    float3 camera_pos;
    float pad00;

    uint2 resolution;
    float camera_near;
    float camera_far;


    float3 sun_direction;
    float pad10;

    float3 sun_illuminance;
    float ambient;
} global;
