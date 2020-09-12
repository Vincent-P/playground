layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;
layout (location = 4) in vec4 inJoint0;
layout (location = 5) in vec4 inWeight0;

layout (location = 0) out vec2 outUV0;

#include "globals.h"

layout (set = 1, binding = 0) uniform UBONode {
    float4x4 nodes_transforms[4]; // max nodes
};

struct GltfPushConstant
{
    // uniform
    u32 node_idx;

    // textures
    u32 base_color_idx;
    u32 normal_map_idx;
    float pad00;

    // material
    float4 base_color_factor;
};

layout(push_constant) uniform GC {
    GltfPushConstant constants;
};


layout (set = 1, binding = 1) uniform CI {
    uint cascade_index;
};

struct CascadeMatrix
{
    float4x4 view;
    float4x4 proj;
};

layout (set = 1, binding = 2) uniform CM {
    CascadeMatrix cascade_matrices[10];
};

void main()
{
    float4x4 transform = nodes_transforms[constants.node_idx];
    vec4 locPos = transform * vec4(inPosition, 1.0);
    CascadeMatrix matrices = cascade_matrices[cascade_index];
    locPos /= locPos.w;
    outUV0 = inUV0;
    gl_Position = matrices.proj * matrices.view * locPos;
    // debug shadow map :)
    // gl_Position = outLightPosition;
}
