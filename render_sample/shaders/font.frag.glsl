#pragma shader_stage(fragment)

#include "types.h"
#include "globals.h"


layout(set = SHADER_SET, binding = 0) uniform Options {
    float2 scale;
    float2 translation;
    u32 vertices_descriptor_index;
    u32 font_atlas_descriptor_index;
};

layout(location = 0) in float2 i_uv;
layout(location = 0) out float4 o_color;
void main()
{
    float alpha = texture(global_textures[font_atlas_descriptor_index], i_uv).r;
    o_color = float4(0, 0, 0, alpha);
}
