#version 450

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inOffset;

layout (location = 0) out vec4 outColor;

void main()
{
    outColor.rgb = inColor;
    outColor.a = 1.0;
}
