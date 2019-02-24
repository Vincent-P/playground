#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (push_constant) uniform Material {
	vec4 baseColorFactor;
	vec4 emissiveFactor;
	vec4 diffuseFactor;
	vec4 specularFactor;
	float workflow;
	float hasBaseColorTexture;
	float hasPhysicalDescriptorTexture;
	float hasNormalTexture;
	float hasOcclusionTexture;
	float hasEmissiveTexture;
	float metallicFactor;
	float roughnessFactor;
	float alphaMask;
	float alphaMaskCutoff;
} material;

layout(location = 0) in vec3 outWorldPos;
layout(location = 1) in vec3 outNormal;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = material.baseColorFactor;
}
