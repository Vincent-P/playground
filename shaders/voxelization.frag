#version 450

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;

struct VoxelData
{
    vec4 color;
    vec4 normal;
};

layout(set = 0, binding = 0) buffer VoxelsBuffer { VoxelData data[]; } voxels;

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

// Find the normal for this fragment, pulling either from a predefined normal map
// or from the interpolated mesh normal and tangent attributes.
vec3 getNormal()
{
    // Perturb normal, see http://www.thetenthplanet.de/archives/1180
    vec3 tangentNormal = texture(normalMap, material.normalTextureSet == 0 ? inUV0 : inUV1).xyz * 2.0 - 1.0;

    vec3 q1 = dFdx(inWorldPos);
    vec3 q2 = dFdy(inWorldPos);
    vec2 st1 = dFdx(inUV0);
    vec2 st2 = dFdy(inUV0);

    vec3 N = normalize(inNormal);
    vec3 T = normalize(q1 * st2.t - q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

 float saturate(float a) { return clamp(a, 0.0f, 1.0f); }
 bool is_saturated(float a) { return a == saturate(a); }
 bool is_saturated(vec2 a) { return is_saturated(a.x) && is_saturated(a.y); }
 bool is_saturated(vec3 a) { return is_saturated(a.x) && is_saturated(a.y) && is_saturated(a.z); }
 bool is_saturated(vec4 a) { return is_saturated(a.x) && is_saturated(a.y) && is_saturated(a.z) && is_saturated(a.w); }

// 3D array index to flattened 1D array index
uint flatten3D (uvec3 coord, uvec3 dim)
{
	return (coord.z * dim.x * dim.y) + (coord.y * dim.x) + coord.x;
}

// flattened array index to 3D array index
uvec3 unflatten3D(uint idx, uvec3 dim)
{
	const uint z = idx / (dim.x * dim.y);
	idx -= (z * dim.x * dim.y);
	const uint y = idx / dim.x;
	const uint x = idx % dim.x;
	return  uvec3(x, y, z);
}

void main()
{
    vec3 diff = (inWorldPos - debug_options.center) / (debug_options.res * debug_options.size);
    vec3 uvw = diff * vec3(0.5f, 0.5f, 0.5f) + 0.5f;

    if (is_saturated(uvw))
    {
        vec4 color;
        vec4 normal = vec4(getNormal(), 1.0);

        if (material.baseColorTextureSet > -1) {
            color = texture(colorMap, material.baseColorTextureSet == 0 ? inUV0 : inUV1) * material.baseColorFactor;
        } else {
            color = material.baseColorFactor;
        }

        // output:
        vec3 writecoord_f = floor(uvw * debug_options.res);
        uvec3 writecoord = uvec3(writecoord_f);
        uint id = flatten3D(writecoord, uvec3(debug_options.res));
        voxels.data[id].color = color;
        voxels.data[id].normal = normal;
    }
}
