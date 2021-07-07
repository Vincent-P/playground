layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inUV;
layout (location = 0) out vec3 outPosition;
layout (location = 1) out vec2 outUV;

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


void main()
{
    CascadeMatrix matrices = cascade_matrices[cascade_index];
    float size = 100.0;
    outUV = inUV * size;
    gl_Position = matrices.proj * matrices.view * float4(inPosition.x*size, inPosition.y, inPosition.z*size,1.0);
}
