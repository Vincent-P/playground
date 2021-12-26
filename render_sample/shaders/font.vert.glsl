#pragma shader_stage(vertex)

#include "base/types.h"
#include "base/constants.h"
#include "render_sample/globals.h"

layout(set = SHADER_SET, binding = 0) uniform Options {
    float2 scale;
    float2 translation;
    u32 vertices_descriptor_index;
    u32 primitive_bytes_offset;
};

void textured_rect(out float2 o_position, out float2 o_uv, u32 i_primitive, u32 corner)
{
    u32 primitive_offset = primitive_bytes_offset / sizeof_textured_rect;
    TexturedRect rect = global_buffers_textured_rects[vertices_descriptor_index].rects[primitive_offset + i_primitive];

    o_position = rect.rect.position;
    o_uv = rect.uv.position;

    // 0 - 3
    // |   |
    // 1 - 2
    if (corner == 1)
    {
        o_position.y += rect.rect.size.y;
        o_uv.y += rect.uv.size.y;
    }
    else if (corner == 2)
    {
        o_position += rect.rect.size;
        o_uv += rect.uv.size;
    }
    else if (corner == 3)
    {
        o_position.x += rect.rect.size.x;
        o_uv.x += rect.uv.size.x;
    }
}

void color_rect(out float2 o_position, out float2 o_uv, u32 i_primitive, u32 corner)
{
    u32 primitive_offset = primitive_bytes_offset / sizeof_color_rect;
    ColorRect rect = global_buffers_color_rects[vertices_descriptor_index].rects[primitive_offset + i_primitive];

    o_position = rect.rect.position;
    o_uv = float2(0.0);

    // 0 - 3
    // |   |
    // 1 - 2
    if (corner == 1)
    {
        o_position.y += rect.rect.size.y;
        o_uv.y = 1.0f;
    }
    else if (corner == 2)
    {
        o_position += rect.rect.size;
        o_uv = float2(1.0f);
    }
    else if (corner == 3)
    {
        o_position.x += rect.rect.size.x;
        o_uv.x = 1.0f;
    }
}

layout(location = 0) out float2 o_uv;
layout(location = 1) out flat u32 o_primitive_index;
void main()
{
    u32 i_primitive    = gl_VertexIndex & 0x00ffffff;
    u32 corner         = (gl_VertexIndex & 0x03000000) >> 24;
    u32 primitive_type = (gl_VertexIndex & 0xfc000000) >> 26;

    float2 position = float2(0.0);
    float2 uv = float2(0.0);

    if (primitive_type == RectType_Textured)
    {
        textured_rect(position, uv, i_primitive, corner);
    }
    else if (primitive_type == RectType_Color)
    {
        color_rect(position, uv, i_primitive, corner);
    }

    gl_Position = float4(position * scale + translation, 0.0, 1.0);
    o_uv = uv;
    o_primitive_index = gl_VertexIndex;
}
