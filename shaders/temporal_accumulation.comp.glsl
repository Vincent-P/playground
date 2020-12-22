#pragma shader_stage(compute)

#include "types.h"
#include "constants.h"
#include "globals.h"
#include "maths.h"

// input frame N
layout (set = 1, binding = 0) uniform sampler2D depth_buffer;
layout (set = 1, binding = 1) uniform sampler2D current_frame;

// history N-1
layout (set = 1, binding = 2) uniform sampler2D previous_history;
// output
layout (set = 1, binding = 3, rgba16f) uniform image2D output_frame;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

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

void main()
{
    uint local_idx = gl_LocalInvocationIndex;
    uint3 global_idx = gl_GlobalInvocationID;
    uint3 group_idx = gl_WorkGroupID;

    if (global_idx.x >= global.resolution.x || global_idx.y >= global.resolution.y)
    {
        // this thread has nothing to do
        return;
    }

    int2 pixel_pos = int2(global_idx.xy);
    float2 uv = float2(pixel_pos) / global.resolution;
    float3 clip_space = float3(uv * 2.0 - float2(1.0), 0.0);
    clip_space.z = max(clip_space.z, 0.000001);

    // unproject and unjitter with current camera to get world pos
    float4x4 current_inv_proj = global.camera_inv_proj;
    current_inv_proj = get_jittered_inv_projection(global.camera_inv_proj, global.jitter_offset);
    float4 world_pos = global.camera_inv_view * current_inv_proj * float4(clip_space, 1.0);
    world_pos /= world_pos.w;

    // project with previous camera settings
    float4x4 previous_proj = global.camera_previous_proj;
    // previous_proj = get_jittered_projection(global.camera_previous_proj, global.previous_jitter_offset);
    float4 previous_history_clip = previous_proj * global.camera_previous_view * world_pos;
    float2 previous_history_uv = 0.5 * (previous_history_clip.xy / previous_history_clip.w) + 0.5;


    /// --- Constraint history
    float4 neighbour_box[9];
#define ITER(i) neighbour_box[i] = texelFetchOffset(current_frame, pixel_pos, LOD0, three_square_offsets[i]);
    ITER(0)
    ITER(1)
    ITER(2)
    ITER(3)
    ITER(4)
    ITER(5)
    ITER(6)
    ITER(7)
    ITER(8)
#undef ITER

    float4 neighbour_cross[5];
    neighbour_cross[0] = neighbour_box[1];
    neighbour_cross[1] = neighbour_box[3];
    neighbour_cross[2] = neighbour_box[4];
    neighbour_cross[3] = neighbour_box[5];
    neighbour_cross[4] = neighbour_box[7];

    float4 color = neighbour_box[4];
    float depth  = texelFetch(depth_buffer, pixel_pos, 0).r;
    float4 history_color = texture(previous_history, previous_history_uv);

    float4 box_max = neighbour_box[0];
    float4 box_min = neighbour_box[0];
    for (uint i = 1; i < 9; i++)
    {
        box_min = min(box_min, neighbour_box[i]);
        box_max = max(box_max, neighbour_box[i]);
    }

    float4 cross_max = neighbour_cross[0];
    float4 cross_min = neighbour_cross[0];
    for (uint i = 1; i < 5; i++)
    {
        cross_min = min(cross_min, neighbour_cross[i]);
        cross_max = max(cross_max, neighbour_cross[i]);
    }

    float4 neighbour_min = (box_min + cross_min) * 0.5;
    float4 neighbour_max = (box_max + cross_max) * 0.5;

    history_color = clip_aabb(rgb_to_ycocg(neighbour_min.rgb),
                              rgb_to_ycocg(neighbour_max.rgb),
                              float4(rgb_to_ycocg(color.rgb), 1.0),
                              float4(rgb_to_ycocg(history_color.rgb), 1.0));

    history_color.rgb = ycocg_to_rgb(history_color.rgb);

    /// --- Blend

    float4 final_color = color;
    // 16 samples => 1/16  weight => 0.0625
    const float blend_weight = 0.0625;

    if (0.0 <= previous_history_uv.x && previous_history_uv.x <= 1.0
        && 0.0 <= previous_history_uv.y && previous_history_uv.y <= 1.0
        && history_color.a > 0.1)
    {
        final_color = blend_weight * color + (1.0 - blend_weight) * history_color;
    }

    final_color.a = 1;

    final_color.rgb = max(final_color.rgb, float3(0, 0, 0));

    imageStore(output_frame, pixel_pos, final_color);
}
