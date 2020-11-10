#include "types.h"
#define PBR_NO_NORMALS
#include "pbr.h"

#extension GL_EXT_nonuniform_qualifier : require

struct GltfVertex
{
    float3 position;
    float pad00;
    float3 normal;
    float pad01;
    float2 uv0;
    float2 uv1;
    float4 joint0;
    float4 weight0;
};

layout(set = 1, binding = 0) buffer GltfVertexBuffer {
    GltfVertex vertices[];
};

layout (set = 1, binding = 1) uniform UBONode {
    float4x4 nodes_transforms[4]; // max nodes
};

layout (location = 0) out float3 out_world_pos;
layout (location = 1) out float3 out_normal;
layout (location = 2) out float2 out_uv0;
layout (location = 3) out float2 out_uv1;

void main()
{
    float4x4 transform = nodes_transforms[constants.node_idx];
    GltfVertex vertex = vertices[gl_VertexIndex + constants.vertex_offset];

    float4 model_pos = transform * float4(vertex.position, 1.0);
    out_normal = normalize(transpose(inverse(mat3(transform))) * vertex.normal);
    out_world_pos = model_pos.xyz / model_pos.w;
    out_uv0 = vertex.uv0;
    out_uv1 = vertex.uv1;

    gl_Position = float4(out_world_pos, 1.0);
}
