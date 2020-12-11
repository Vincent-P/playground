#include "types.h"

layout(set = 1, binding = 0) uniform sampler2D hdr_buffer;

layout(set = 1, binding = 1) uniform DO
{
    uint selected;
    float exposure;
} debug;

layout (set = 1, binding = 2, r32f) uniform readonly image2D luminance_output;

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;


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

void main()
{
    float3 hdr = texture(hdr_buffer, inUV).rgb;
    float3 ldr = float3(0.0);

    float average_luminance = imageLoad(luminance_output, int2(0, 0)).r;
    float3 exposure_adjusted = hdr / (9.6 * average_luminance);

    // reinhard
    if (debug.selected == 0)
    {
        ldr = exposure_adjusted / (exposure_adjusted + 1.0);
    }
    // exposure
    else if (debug.selected == 1)
    {
        ldr = vec3(1.0) - exp(-hdr * debug.exposure);
    }
    // clamp
    else if (debug.selected == 2)
    {
        ldr = clamp(hdr, 0.0, 1.0);
    }
    // ACES?
    else if (debug.selected == 3)
    {
        ldr = ACESFitted(exposure_adjusted);
    }
    else
    {
        ldr = float3(1, 0, 0);
    }

    // to srgb
    ldr = pow(ldr, float3(1.0 / 2.2));

    outColor = vec4(ldr, 1.0);
}
