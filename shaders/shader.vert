#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;

layout(set = 0, binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
    mat4 clip;
    vec4 cam_pos;
    vec4 light_dir;
    float debugViewInput;
    float debugViewEquation;
} ubo;

layout (set = 2, binding = 0) uniform UBONode {
    mat4 matrix;
} node;

layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outUV0;
layout (location = 3) out vec2 outUV1;

void main() {
    vec4 locPos = node.matrix * vec4(inPosition, 1.0);
    outNormal = normalize(transpose(inverse(mat3(node.matrix))) * inNormal);
    outWorldPos = locPos.xyz / locPos.w;
    outUV0 = inUV0;
    outUV1 = inUV1;

    gl_Position =  ubo.clip * ubo.proj * ubo.view * vec4(outWorldPos, 1.0);
}
