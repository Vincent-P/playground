#include "types.h"
#include "globals.h"

layout(set = 1, binding = 0) uniform Options {
    u32 padd0;
    u32 padd1;
};

layout(set = 1, binding = 1) buffer VertexBuffer {
    Vertex vertices[];
};

layout(set = 1, binding = 2) buffer RenderMeshesBuffer {
    RenderMeshData render_meshes[];
};

layout (location = 0) out float4 o_vertex_color;
layout (location = 1) out float2 o_uv;
layout (location = 2) out float4 o_world_pos;
layout (location = 3) out float3 o_normal;
void main()
{
    RenderMeshData render_mesh = render_meshes[push_constants.render_mesh_idx];
    Vertex vertex = vertices[gl_VertexIndex];

    float4 world_pos = render_mesh.transform * float4(vertex.position, 1.0);
    gl_Position = globals.camera_projection * globals.camera_view * world_pos;
    o_vertex_color = vertex.color0;
    o_uv = vertex.uv0;
    o_world_pos = world_pos;
    o_normal = vertex.normal;
}
