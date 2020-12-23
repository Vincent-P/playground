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

    if (global_idx.x >= global.resolution.x || global_idx.y >= global.resolution.y)
    {
        // this thread has nothing to do
        return;
    }

    int2 pixel_pos = int2(global_idx.xy);
    float depth  = texelFetch(depth_buffer, pixel_pos, 0).r;

    float2 uv = float2(pixel_pos) / global.resolution;
    float3 clip_space = float3(2.0 * uv - float2(1.0), max(depth, 0.00001));



    /// --- Reproject: current frame to get the previous frame

    // unproject with current camera to get world pos
    float4x4 current_inv_proj = get_jittered_inv_projection(global.camera_inv_proj, global.jitter_offset);
    float4 world_pos = global.camera_inv_view * current_inv_proj * float4(clip_space, 1.0);
    world_pos /= world_pos.w;

    // project with previous camera settings
    float4x4 previous_proj = global.camera_previous_proj;
    // previous_proj = get_jittered_projection(global.camera_proj, global.previous_jitter_offset);
    float4 previous_history_clip = previous_proj * global.camera_view * world_pos;
    previous_history_clip /= previous_history_clip.w;
    float2 previous_history_uv = 0.5 * previous_history_clip.xy + 0.5;

    float4 history_color = float4(0, 0, 0, 1);
    if (0.0 < previous_history_uv.x && previous_history_uv.x < 1.0
        && 0.0 < previous_history_uv.y && previous_history_uv.y < 1.0)
    {
        history_color = texture(previous_history, previous_history_uv);
    }


    /// --- Min Max: average of a 3x3 box and 5 taps cross

    float4 neighbour_box[9] =
    {
        texelFetchOffset(current_frame, pixel_pos, LOD0, three_square_offsets[0]),
        texelFetchOffset(current_frame, pixel_pos, LOD0, three_square_offsets[1]),
        texelFetchOffset(current_frame, pixel_pos, LOD0, three_square_offsets[2]),
        texelFetchOffset(current_frame, pixel_pos, LOD0, three_square_offsets[3]),
        texelFetchOffset(current_frame, pixel_pos, LOD0, three_square_offsets[4]),
        texelFetchOffset(current_frame, pixel_pos, LOD0, three_square_offsets[5]),
        texelFetchOffset(current_frame, pixel_pos, LOD0, three_square_offsets[6]),
        texelFetchOffset(current_frame, pixel_pos, LOD0, three_square_offsets[7]),
        texelFetchOffset(current_frame, pixel_pos, LOD0, three_square_offsets[8])
    };

    float4 color = neighbour_box[4];

    float4 neighbour_cross[5] =
    {
        neighbour_box[1],
        neighbour_box[3],
        neighbour_box[4],
        neighbour_box[5],
        neighbour_box[7]
    };

    float4 cross_max = neighbour_cross[0];
    float4 cross_min = neighbour_cross[0];
    for (uint i = 1; i < 5; i++)
    {
        cross_min = min(cross_min, neighbour_cross[i]);
        cross_max = max(cross_max, neighbour_cross[i]);
    }

    float4 box_min = min(cross_min, neighbour_box[0]);
    box_min = min(box_min, neighbour_box[2]);
    box_min = min(box_min, neighbour_box[6]);
    box_min = min(box_min, neighbour_box[8]);

    float4 box_max = max(cross_max, neighbour_box[0]);
    box_max = max(box_max, neighbour_box[2]);
    box_max = max(box_max, neighbour_box[6]);
    box_max = max(box_max, neighbour_box[8]);

    float4 neighbour_min = (box_min + cross_min) * 0.5;
    float4 neighbour_max = (box_max + cross_max) * 0.5;



    /// --- Constrain history to avoid ghosting

    #if 0
    // clip the history color in YCoCg space
    history_color = clip_aabb(rgb_to_ycocg(neighbour_min.rgb),
                              rgb_to_ycocg(neighbour_max.rgb),
                              float4(rgb_to_ycocg(color.rgb), 1.0),
                              float4(rgb_to_ycocg(history_color.rgb), 1.0));
    history_color.rgb = ycocg_to_rgb(history_color.rgb);
    #elif 1
    // clip the history color in RGB space
    history_color = clip_aabb(neighbour_min.rgb,
                              neighbour_max.rgb,
                              color,
                              history_color);
    #endif



    /// --- Unjitter current frame color




    /// --- Weigh the history and the current frame to the new history

    const float blend_weight = 0.01;

    float4 final_color = color;

    if (0.0 < previous_history_uv.x && previous_history_uv.x < 1.0
        && 0.0 < previous_history_uv.y && previous_history_uv.y < 1.0
        && history_color.a > 0.1)
    {
        final_color = mix(history_color, color, blend_weight);
    }

    imageStore(output_frame, pixel_pos, final_color);
}
