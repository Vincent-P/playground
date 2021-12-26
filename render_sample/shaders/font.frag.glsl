#pragma shader_stage(fragment)

#include "base/types.h"
#include "render_sample/globals.h"

layout(set = SHADER_SET, binding = 0) uniform Options {
    float2 scale;
    float2 translation;
    u32 vertices_descriptor_index;
    u32 primitive_bytes_offset;
};

float4 textured_rect(u32 i_primitive, u32 corner, float2 uv)
{
    u32 primitive_offset = primitive_bytes_offset / sizeof_textured_rect;
    TexturedRect rect = global_buffers_textured_rects[vertices_descriptor_index].rects[primitive_offset + i_primitive];

    float alpha = texture(global_textures[nonuniformEXT(rect.texture_descriptor)], uv).r;

    return float4(0, 0, 0, alpha);
}

layout(location = 0) in float2 i_uv;
layout(location = 1) in flat u32 i_primitive_index;
layout(location = 0) out float4 o_color;
void main()
{
    u32 i_primitive    = i_primitive_index & 0x00ffffff;
    u32 corner         = (i_primitive_index & 0x03000000) >> 24;
    u32 primitive_type = (i_primitive_index & 0xfc000000) >> 26;

    if (primitive_type == RectType_Textured)
    {
        o_color = textured_rect(i_primitive, corner, i_uv);
    }
    else
    {
        o_color = float4(1, 0, 0, 1);
    }
}
