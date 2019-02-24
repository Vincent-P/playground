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

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 lightDir = vec3(1.0, 1.0, 0.0);
    vec3 lightColor = vec3(1.0, 1.0, 1.0);

    float diff = max(dot(inNormal, lightDir), 0.0);

    vec3 diffuse = diff * lightColor;
    vec3 ambient = 0.6 * lightColor;

    vec3 result = (ambient + diffuse) * material.baseColorFactor.xyz;
    outColor = vec4(result, 1.0);
}
