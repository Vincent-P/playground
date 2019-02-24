#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(set = 0, binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 clip;
} ubo;

layout (set = 1, binding = 0) uniform UBONode {
    mat4 matrix;
} node;

layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec3 outNormal;

void main() {
    /*
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    */

    vec4 locPos = ubo.model * node.matrix * vec4(inPosition, 1.0);
    outNormal = normalize(transpose(inverse(mat3(ubo.model * node.matrix))) * inNormal);
    outWorldPos = locPos.xyz / locPos.w;
    gl_Position =  ubo.clip * ubo.proj * ubo.view * vec4(outWorldPos, 1.0);

}
