#version 450

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inOffset;

layout(set = 0, binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
    mat4 clip;
    vec4 cam_pos;
    vec4 light_dir;
    float debugViewInput;
    float debugViewEquation;
    float ambient;
    float dummy;
} ubo;

layout (location = 0) out vec4 outColor;

void main()
{
    outColor.rgb = inColor;
    outColor.a = 1.0;
}
