#pragma shader_stage(fragment)

#include "base/types.h"
#include "biv/globals.h"

layout(set = SHADER_UNIFORM_SET, binding = 0) uniform Options {
    float2 scale;
    float2 translation;
    u32 texture_descriptor;
};

layout(location = 0) in float2 i_uv;
layout(location = 0) out float4 o_color;
void main()
{
    float4 sample = texture(global_textures[texture_descriptor], i_uv);
    o_color = float4(sample.rgb, 1);
}
