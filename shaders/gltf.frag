#version 450
#include "pbr.h"

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;
layout (location = 4) in vec4 inJoint0;
layout (location = 5) in vec4 inWeight0;
layout (location = 6) in vec4 inLightPosition;

layout (push_constant) uniform MU
{
    MaterialUniform material;
};

layout (set = 1, binding = 1) uniform UBODebug {
    uint selected;
    float opacity;
} debug;

layout(set = 1, binding = 2) uniform sampler2D shadowMapTexture;

layout(set = 2, binding = 1) uniform sampler2D baseColorTexture;
layout(set = 2, binding = 2) uniform sampler2D normalTexture;
layout(set = 2, binding = 3) uniform sampler2D metallicRoughnessTexture;

layout (location = 0) out vec4 outColor;

float ShadowCalculation(vec4 fragPosLightSpace)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    float currentDepth = projCoords.z;
    projCoords = projCoords * 0.5 + 0.5;
    float closestDepth = texture(shadowMapTexture, projCoords.xy).r;
    float shadow_bias = 0.005;
    float shadow = currentDepth - shadow_bias > closestDepth  ? 1.0 : 0.0;
    return shadow;
}

float shadowDepthDebug(vec4 fragPosLightSpace)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    float closestDepth = texture(shadowMapTexture, projCoords.xy).r;
    return closestDepth;
}

float currentDepthDebug(vec4 fragPosLightSpace)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    return projCoords.z;
}

void main()
{
    vec3 light_dir = normalize(vec3(5, 20, 2));
    vec3 normal = getNormal(inWorldPos, inNormal, normalTexture, inUV0);
    vec4 base_color = texture(baseColorTexture, inUV0);
    if (base_color.a < 0.5) {
        discard;
    }
    vec3 ambient = 0.2 * base_color.rgb;
    float diffuse = max(dot(normal, light_dir), 0.0f);
    float is_in_shadow = ShadowCalculation(inLightPosition);

    vec3 color = (ambient + (1.0 - is_in_shadow) * diffuse) * base_color.rgb;

    switch (debug.selected)
    {
        case 0: outColor = vec4(color, debug.opacity); break;
        case 1: outColor = vec4(texture(baseColorTexture, inUV0).xyz, 1); break;
        case 2: outColor = vec4(abs(normal), 1); break;
        case 3: outColor = vec4(texture(metallicRoughnessTexture, inUV0).xyz, 1); break;
        case 4: {
            float depth = shadowDepthDebug(inLightPosition);
            outColor = vec4(depth, depth, depth, 1);
            break;
            }
        case 5: {
            float depth = currentDepthDebug(inLightPosition);
            outColor = vec4(depth, depth, depth, 1);
            break;
            }
        case 6: {
            float depth = ShadowCalculation(inLightPosition);
            outColor = vec4(depth, depth, depth, 1);
            break;
            }
    }
}
