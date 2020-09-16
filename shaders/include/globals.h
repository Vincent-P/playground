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
    float2 pad01;


    float3 sun_direction;
    float pad10;

    float3 sun_illuminance;
    float ambient;
} global;
