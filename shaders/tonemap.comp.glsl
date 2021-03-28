#pragma shader_stage(compute)
#include "types.h"
#include "constants.h"
#include "globals.h"

layout(set = 1, binding = 0) uniform Options {
    uint sampled_hdr_buffer;
    uint sampled_luminance_output;
    uint storage_output_frame;
    uint selected;
    float exposure;
};

layout(set = 1, binding = 1, rgba8) uniform image2D output_frame;

const float3x3 ACESInputMat =
{
    {0.59719, 0.35458, 0.04823},
    {0.07600, 0.90834, 0.01566},
    {0.02840, 0.13383, 0.83777}
};

// ODT_SAT => XYZ => D60_2_D65 => sRGB
const float3x3 ACESOutputMat =
{
    { 1.60475, -0.53108, -0.07367},
    {-0.10208,  1.10813, -0.00605},
    {-0.00327, -0.07276,  1.07602}
};

float3 RRTAndODTFit(float3 v)
{
    float3 a = v * (v + 0.0245786f) - 0.000090537f;
    float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

float3 ACESFitted(float3 color)
{
    color = color * ACESInputMat;

    // Apply RRT and ODT
    color = RRTAndODTFit(color);

    color = color * ACESOutputMat;

    // Clamp to [0, 1]
    color = clamp(color, 0.0, 1.0);

    return color;
}

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main()
{
    uint local_idx   = gl_LocalInvocationIndex;
    uint3 global_idx = gl_GlobalInvocationID;
    uint3 group_idx  = gl_WorkGroupID;

    int2 pixel_pos = int2(global_idx.xy);
    int2 output_size = imageSize(output_frame);

    if (any(greaterThan(pixel_pos, output_size)))
    {
        return;
    }

    float3 hdr = texelFetch(global_textures[sampled_hdr_buffer], pixel_pos, LOD0).rgb;
    float3 ldr = float3(0.0);

    float average_luminance = texelFetch(global_textures[sampled_luminance_output], int2(0, 0), LOD0).r;
    float3 exposure_adjusted = hdr / (9.6 * average_luminance);

    // reinhard
    if (selected == 0)
    {
        ldr = exposure_adjusted / (exposure_adjusted + 1.0);
    }
    // exposure
    else if (selected == 1)
    {
        ldr = vec3(1.0) - exp(-hdr * exposure);
    }
    // clamp
    else if (selected == 2)
    {
        ldr = clamp(hdr, 0.0, 1.0);
    }
    // ACES?
    else if (selected == 3)
    {
        ldr = ACESFitted(exposure_adjusted);
    }
    else
    {
        ldr = float3(1, 0, 0);
    }

    // to srgb
    ldr = pow(ldr, float3(1.0 / 2.2));


    float4 output_color = vec4(ldr, 1.0);

    imageStore(output_frame, pixel_pos, output_color);
}
