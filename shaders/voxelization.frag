#version 450

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;

layout(set = 0, binding = 0, RGBA8) uniform image3D voxels_texture;

layout(set = 1, binding = 0) uniform UBO {
    vec3 center;
    float size;
    uint res;
} debug_options;

layout (set = 3, binding = 0) uniform sampler2D colorMap;
layout (set = 3, binding = 1) uniform sampler2D physicalDescriptorMap;
layout (set = 3, binding = 2) uniform sampler2D normalMap;
layout (set = 3, binding = 3) uniform sampler2D aoMap;
layout (set = 3, binding = 4) uniform sampler2D emissiveMap;

layout (push_constant) uniform Material
{
    vec4 baseColorFactor;
    vec4 emissiveFactor;
    vec4 diffuseFactor;
    vec4 specularFactor;
    float workflow;
    int baseColorTextureSet;
    int physicalDescriptorTextureSet;
    int normalTextureSet;
    int occlusionTextureSet;
    int emissiveTextureSet;
    float metallicFactor;
    float roughnessFactor;
    float alphaMask;
    float alphaMaskCutoff;
} material;

 float saturate(float a) { return clamp(a, 0.0f, 1.0f); }
 bool is_saturated(float a) { return a == saturate(a); }
 bool is_saturated(vec2 a) { return is_saturated(a.x) && is_saturated(a.y); }
 bool is_saturated(vec3 a) { return is_saturated(a.x) && is_saturated(a.y) && is_saturated(a.z); }
 bool is_saturated(vec4 a) { return is_saturated(a.x) && is_saturated(a.y) && is_saturated(a.z) && is_saturated(a.w); }

void main()
{
    vec3 center = floor(debug_options.center);
    vec3 diff = (inWorldPos - center) / (debug_options.res * debug_options.size);
    vec3 uvw = diff * vec3(0.5f, 0.5f, 0.5f) + 0.5f;

    vec4 color;

    if (material.baseColorTextureSet > -1) {
        color = texture(colorMap, material.baseColorTextureSet == 0 ? inUV0 : inUV1) * material.baseColorFactor;
    } else {
        color = material.baseColorFactor;
    }

    // output:
    vec3 pos = floor(uvw * debug_options.res);
    imageStore(voxels_texture, ivec3(pos), color);
}
