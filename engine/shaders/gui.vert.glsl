#pragma shader_stage(vertex)

#include "types.h"
#include "globals.h"

layout(set = SHADER_SET, binding = 0) uniform Options {
    float2 scale;
    float2 translation;
    u64 vertices_ptr_ptr;
    u32 first_vertex;
    u32 vertices_descriptor_index;
    uvec4 texture_binding_per_draw[64/4];
};

layout(location = 0) out float2 o_uv;
layout(location = 1) out float4 o_color;
void main()
{
    ImGuiVertex vertex = global_buffers_ui_vert[vertices_descriptor_index].vertices[first_vertex + gl_VertexIndex];

    gl_Position = float4( vertex.position * scale + translation, 0.0, 1.0 );
    o_uv        = vertex.uv;
    o_color     = unpackUnorm4x8(vertex.color);
}
