#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec3 outNormal;

void main() {
    // gl_Position = ubo.proj * ubo.view * ubo.model * inPosition;

    vec4 locPos = ubo.model * vec4(inPosition, 1.0);
    outNormal = normalize(transpose(inverse(mat3(ubo.model))) * inNormal);

    locPos.y = -locPos.y;
    outWorldPos = locPos.xyz / locPos.w;
    gl_Position =  ubo.proj * ubo.view * vec4(outWorldPos, 1.0);
}
