#pragma shader_stage(vertex)

#include "types.h"
#include "globals.h"

layout(set = SHADER_SET, binding = 0) uniform Options {
    float2 scale;
    float2 translation;
    u32 vertices_descriptor_index;
    u32 font_atlas_descriptor_index;
};

layout(location = 0) out float2 o_uv;
void main()
{
    FontVertex vertex = global_buffers_font_vert[vertices_descriptor_index].vertices[gl_VertexIndex];

    gl_Position = float4(vertex.position * scale + translation, 0.0, 1.0);
    o_uv = vertex.uv;
}
