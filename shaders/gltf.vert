#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;
layout (location = 4) in vec4 inJoint0;
layout (location = 5) in vec4 inWeight0;

layout (location = 0) out vec3 outPosition;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outUV0;
layout (location = 3) out vec2 outUV1;
layout (location = 4) out vec4 outJoint0;
layout (location = 5) out vec4 outWeight0;

layout (set = 0, binding = 0) uniform UBONode {
    mat4 view;
    mat4 proj;
    mat4 clip;
} node;

void main()
{
    vec4 locPos = vec4(inPosition, 1.0);
    outNormal = inNormal;
    outPosition = locPos.xyz / locPos.w;
    outUV0 = inUV0;
    outUV1 = inUV1;
    outJoint0 = inJoint0;
    outWeight0 = inWeight0;

    outPosition.z *= -1;
    gl_Position = node.clip * node.proj * node.view * vec4(outPosition, 1.0);
}
