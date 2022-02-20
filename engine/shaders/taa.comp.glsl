#pragma shader_stage(compute)

#include "base/types.h"
#include "base/constants.h"
#include "engine/globals.h"

float3 clip_aabb(float3 aabb_min, float3 aabb_max, float3 color)
{
    // note: only clips towards aabb center (but fast!)
    float3 center = 0.5 * (aabb_max + aabb_min);
    float3 extents = 0.5 * (aabb_max - aabb_min) + 0.0001;

    float3 v_clip = color - center;
    float3 v_unit = v_clip.xyz / extents;
    float3 a_unit = abs(v_unit);
    float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

    if (ma_unit > 1.0)
        return center + v_clip / ma_unit;
    else
        return color;// point inside aabb
}

float mitchell_netravali(float x) {
    float B = 1.0 / 3.0;
    float C = 1.0 / 3.0;

    float ax = abs(x);
    if (ax < 1) {
        return ((12 - 9 * B - 6 * C) * ax * ax * ax + (-18 + 12 * B + 6 * C) * ax * ax + (6 - 2 * B)) / 6;
    } else if ((ax >= 1) && (ax < 2)) {
        return ((-B - 6 * C) * ax * ax * ax + (6 * B + 30 * C) * ax * ax + (-12 * B - 48 * C) * ax + (8 * B + 24 * C)) / 6;
    } else {
        return 0;
    }
}


layout(set = SHADER_UNIFORM_SET, binding = 0) uniform Options {
    uint sampled_hdr_buffer;
    uint sampled_depth_buffer;
    uint sampled_previous_history;
    uint storage_current_history;
};

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main()
{
    uint local_idx   = gl_LocalInvocationIndex;
    uint3 global_idx = gl_GlobalInvocationID;
    uint3 group_idx  = gl_WorkGroupID;

    int2 pixel_pos = int2(global_idx.xy);
    int2 output_size = imageSize(global_images_2d_rgba32f[storage_current_history]);
    int2 input_size  = textureSize(global_textures[sampled_hdr_buffer], LOD0);

    if (any(greaterThan(pixel_pos, output_size)))
    {
        return;
    }

    if (globals.enable_taa == false)
    {
	imageStore(global_images_2d_rgba32f[storage_current_history], pixel_pos, texelFetch(global_textures[sampled_hdr_buffer], pixel_pos, LOD0));
	return;
    }

    float depth = texelFetch(global_textures[sampled_depth_buffer], pixel_pos, LOD0).r;

    float2 uv = (float2(pixel_pos) + float2(0.5)) / float2(output_size);
    float4 clip_space = float4(uv * 2.0 - 1.0, depth, 1.0);

    // Compute motion vector from the camera movement only
    float4x4 inv_proj = globals.camera_projection_inverse;
    inv_proj[3][0] = globals.jitter_offset.x / input_size.x * inv_proj[0][0];
    inv_proj[3][1] = globals.jitter_offset.x / input_size.y * inv_proj[1][1];
    float4 world_pos = globals.camera_view_inverse * inv_proj * clip_space;
    float4 previous_clip_space = globals.camera_previous_projection * globals.camera_previous_view * world_pos;
    float2 previous_uv = previous_clip_space.xy / previous_clip_space.w;
    previous_uv = previous_uv * 0.5  + 0.5;
    // snap to texel center
    previous_uv = (floor(previous_uv * input_size) + float2(0.5))  / float2(input_size);
    float2 motion = previous_uv - uv;

    // --
    float3 sample_sum = float3(0, 0, 0);
    float sample_weight = 0.0;
    float3 neighbor_min = float3(10000);
    float3 neighbor_max = -float3(10000);
    float3 m1 = float3(0, 0, 0);
    float3 m2 = float3(0, 0, 0);
    float closest_depth = 0.0;
    int2 closest_depth_pixel = int2(0, 0);

    const int NB_S = 3;
    for (int x = -NB_S; x <= NB_S; x++)
    {
        for (int y = -NB_S; y <= NB_S; y++)
        {
            int2 neighbor_pos = pixel_pos + int2(x, y);
            neighbor_pos = clamp(neighbor_pos, int2(0), input_size - int2(1));

            float3 neighbor = max(float3(0.0), texelFetch(global_textures[sampled_hdr_buffer], neighbor_pos, LOD0).rgb);
            float neighbor_dist = length(float2(x, y));
            float neighbor_weight = mitchell_netravali(neighbor_dist);

            sample_sum += neighbor * neighbor_weight;
            sample_weight += neighbor_weight;

            neighbor_min = min(neighbor_min, neighbor);
            neighbor_max = max(neighbor_max, neighbor);

            m1 += neighbor;
            m2 += neighbor * neighbor;

            float neighbor_depth = texelFetch(global_textures[sampled_depth_buffer], neighbor_pos, LOD0).r;
            if (neighbor_depth > closest_depth)
            {
                closest_depth = neighbor_depth;
                closest_depth_pixel = neighbor_pos;
            }
        }
    }

    float3 hdr_color = sample_sum / sample_weight;

    // --

    float3 history_color = texture(global_textures[sampled_previous_history], uv + motion).rgb;

    float one_over_count = 1.0 / float((1 + 2 * NB_S) * (1 + 2 * NB_S));
    const float gamma = 1.0;
    float3 mu = m1 * one_over_count;
    float3 sigma = sqrt(abs((m2 * one_over_count) - (mu * mu)));
    float3 minc = mu - gamma * sigma;
    float3 maxc = mu + gamma * sigma;

    history_color = clip_aabb(minc, maxc, clamp(history_color, neighbor_min, neighbor_max));

    float weight = 0.1;

    float3 output_color = mix(history_color, hdr_color, weight);

    imageStore(global_images_2d_rgba32f[storage_current_history], pixel_pos, float4(output_color, 1.0));
}
