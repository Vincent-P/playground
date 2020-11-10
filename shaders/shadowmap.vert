layout (location = 0) out vec2 out_uv0;

#include "globals.h"
#define PBR_NO_NORMALS
#include "pbr.h"

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

layout (set = 1, binding = 2) uniform CI {
    uint cascade_index;
};

struct CascadeMatrix
{
    float4x4 view;
    float4x4 proj;
};

layout (set = 1, binding = 3) uniform CM {
    CascadeMatrix cascade_matrices[10];
};

void main()
{
    // fetch vertex data
    float4x4 model_transform = nodes_transforms[constants.node_idx];
    GltfVertex vertex = vertices[gl_VertexIndex + constants.vertex_offset];

    CascadeMatrix matrices = cascade_matrices[cascade_index];
    out_uv0 = vertex.uv0;
    gl_Position = matrices.proj * matrices.view * model_transform * vec4(vertex.position, 1.0);
    // debug shadow map :)
    // gl_Position = outLightPosition;
}
