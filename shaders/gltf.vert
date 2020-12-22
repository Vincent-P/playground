#include "types.h"

layout (location = 0) out float3 out_position;
layout (location = 1) out float3 out_normal;
layout (location = 2) out float2 out_uv0;
layout (location = 3) out float2 out_uv1;
layout (location = 4) out float4 out_color0;
layout (location = 5) out float4 out_joint0;
layout (location = 6) out float4 out_weight0;

#include "globals.h"
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
    float4 color0;
    float4 joint0;
    float4 weight0;
};

layout(set = 1, binding = 0) buffer GltfVertexBuffer {
    GltfVertex vertices[];
};

layout (set = 1, binding = 1) uniform UBONode {
    float4x4 nodes_transforms[4]; // max nodes
};


void main()
{
    float4x4 transform = nodes_transforms[constants.node_idx];
    GltfVertex vertex = vertices[gl_VertexIndex + constants.vertex_offset];

    float4 locPos = transform * float4(vertex.position, 1.0);
    out_normal = normalize(transpose(inverse(mat3(transform))) * vertex.normal);
    out_position = locPos.xyz / locPos.w;
    out_uv0 = vertex.uv0;
    out_uv1 = vertex.uv1;
    out_color0 = vertex.color0;
    out_joint0 = vertex.joint0;
    out_weight0 = vertex.weight0;
    gl_Position = get_jittered_projection(global.camera_proj, global.jitter_offset) * global.camera_view * float4(out_position, 1.0);
    // debug shadow map :)
    // gl_Position = outLightPosition;
}
