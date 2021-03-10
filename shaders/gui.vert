#extension GL_ARB_shader_draw_parameters : require
#extension GL_EXT_buffer_reference : require

#include "types.h"
#include "globals.h"

struct ImGuiVertex
{
    float2 position;
    float2 uv;
    uint color;
    uint pad00;
};

layout(buffer_reference) buffer VerticesType {
    ImGuiVertex vertices[];
};

layout(set = 1, binding = 0) uniform Options {
    float2 scale;
    float2 translation;
    VerticesType vertices_ptr;
    uint texture_binding;
};

layout(location = 0) out float2 o_uv;
layout(location = 1) out float4 o_color;
void main()
{
    ImGuiVertex vertex = vertices_ptr.vertices[gl_VertexIndex];

    gl_Position = float4( vertex.position * scale + translation, 0.0, 1.0 );
    o_uv        = vertex.uv;
    o_color     = unpackUnorm4x8(vertex.color);
}
