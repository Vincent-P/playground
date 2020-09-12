layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;
layout (location = 4) in vec4 inJoint0;
layout (location = 5) in vec4 inWeight0;

layout (location = 0) out vec2 outUV0;

#include "globals.h"

layout (set = 1, binding = 0) uniform CI {
    uint cascade_index;
};

struct CascadeMatrix
{
    float4x4 view;
    float4x4 proj;
};

layout (set = 1, binding = 1) uniform CM {
    CascadeMatrix cascade_matrices[10];
};

layout (set = 2, binding = 0) uniform UBONode {
    mat4 transform;
} node;

void main()
{
    vec4 locPos = node.transform * vec4(inPosition, 1.0);
    CascadeMatrix matrices = cascade_matrices[cascade_index];
    locPos /= locPos.w;
    outUV0 = inUV0;
    gl_Position = matrices.proj * matrices.view * locPos;
    // debug shadow map :)
    // gl_Position = outLightPosition;
}
