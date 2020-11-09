#include "types.h"
#define PBR_NO_NORMALS
#include "pbr.h"

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;
layout (location = 4) in vec4 inJoint0;
layout (location = 5) in vec4 inWeight0;

#extension GL_EXT_nonuniform_qualifier : require

layout (set = 1, binding = 0) uniform UBONode {
    float4x4 nodes_transforms[4]; // max nodes
};

layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outUV0;
layout (location = 3) out vec2 outUV1;

void main()
{
    float4x4 transform = nodes_transforms[constants.node_idx];
    vec4 locPos = transform * vec4(inPosition, 1.0);
    outNormal = normalize(transpose(inverse(mat3(transform))) * inNormal);
    outWorldPos = locPos.xyz / locPos.w;
    outUV0 = inUV0;
    outUV1 = inUV1;

    gl_Position = vec4(outWorldPos, 1.0);
}
