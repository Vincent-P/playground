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

void main()
{
    float3 ldr = texture(hdr_buffer, inUV).rgb;
    float3 hdr = float3(0.0);

    // exposure
    if (debug.selected == 1)
    {
        hdr = vec3(1.0) - exp(-ldr * debug.exposure);
    }
    // clamp
    else if (debug.selected == 2)
    {
        hdr = clamp(ldr, 0.0, 1.0);
    }
    // reinhard
    else
    {
        float average_luminance = imageLoad(luminance_output, int2(0, 0)).r;
        float3 exposure_adjusted = ldr / (9.6 * average_luminance);
        hdr = exposure_adjusted / (exposure_adjusted + 1.0);
    }

    // to srgb
    hdr = pow(hdr, float3(1.0 / 2.2));

    outColor = vec4(hdr, 1.0);
}
