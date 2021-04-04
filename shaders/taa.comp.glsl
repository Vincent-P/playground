#pragma shader_stage(compute)

#include "types.h"
#include "constants.h"
#include "globals.h"
#include "maths.h"

layout (set = 1, binding = 1, rgba32f) uniform image2D output_frame;

layout(set = 1, binding = 0) uniform Options {
    uint sampled_depth_buffer;
    uint sampled_hdr_buffer;
    uint sampled_previous_history;
};

const int2 three_square_offsets[9] =
{
    int2(-1, -1),
    int2( 0, -1),
    int2( 1, -1),
    int2(-1,  0),
    int2( 0,  0),
    int2( 1,  0),
    int2(-1,  1),
    int2( 0,  1),
    int2( 1,  1)
};

// note: clips towards aabb center + p.w
float4 clip_aabb(
    float3 aabb_min, // cn_min
    float3 aabb_max, // cn_max
    float4 p,        // c_in’
    float4 q)        // c_hist
{
    float3 p_clip = 0.5 * (aabb_max + aabb_min);
    float3 e_clip = 0.5 * (aabb_max - aabb_min);
    float4 v_clip = q - float4(p_clip, p.w);
    float3 v_unit = v_clip.xyz / e_clip;
    float3 a_unit = abs(v_unit);
    float ma_unit = max3(a_unit);

    if (ma_unit > 1.0)
        return float4(p_clip, p.w) + v_clip / ma_unit;
    else
        return q;// point inside aabb
}

float3 rgb_to_ycocg(float3 rgb)
{
    float3x3 m = float3x3(
        0.25, 0.5,  0.25,
        0.50, 0.0, -0.50,
       -0.25, 0.5, -0.25
        );
    return m * rgb;
}

float3 ycocg_to_rgb(float3 ycocg)
{
    float3x3 m = float3x3(
        1,  1, -1,
        1,  0,  1,
        1, -1, -1
        );
    return m * ycocg;
}

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main()
{
    uint local_idx = gl_LocalInvocationIndex;
    uint3 global_idx = gl_GlobalInvocationID;
    uint3 group_idx = gl_WorkGroupID;

    if (global_idx.x >= globals.resolution.x || global_idx.y >= globals.resolution.y)
    {
        // this thread has nothing to do
        return;
    }

    int2 pixel_pos = int2(global_idx.xy);

    float4 history_color = texelFetch(global_textures[sampled_previous_history], pixel_pos, LOD0);
    float4 color = texelFetch(global_textures[sampled_hdr_buffer], pixel_pos, LOD0);

    // avoid nan
    history_color = clamp(history_color, 0.0, 1000000000000000000.0);
    color = clamp(color, 0.0, 1000000000000000000.0);


    float blend_weight = globals.camera_moved != 0 ? 1.0 : 1.0f / (1.0f + (1.0f / history_color.a));
    // blend_weight = 1.0;

    float3 final_color = mix(history_color.rgb, color.rgb, blend_weight);

    imageStore(output_frame, pixel_pos, float4(final_color, blend_weight));
}
