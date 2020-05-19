#include "constants.h"
#include "pbr.h"
#include "voxels.h"

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;
layout (location = 4) in vec4 inJoint0;
layout (location = 5) in vec4 inWeight0;

layout (push_constant) uniform MU
{
    MaterialUniform material;
};

layout (set = 1, binding = 1) uniform UBODebug {
    uint selected;
    float opacity;
} debug;

layout (set = 1, binding = 2) uniform VO {
    VoxelOptions voxel_options;
};

layout(set = 1, binding = 3) uniform sampler3D voxels_radiance;
layout(set = 1, binding = 4) uniform sampler3D voxels_directional_volumes[6];

layout(set = 2, binding = 1) uniform sampler2D baseColorTexture;
layout(set = 2, binding = 2) uniform sampler2D normalTexture;
layout(set = 2, binding = 3) uniform sampler2D metallicRoughnessTexture;

layout (location = 0) out vec4 outColor;


const vec3 diffuseConeDirections[] =
{
    vec3(0.0f, 1.0f, 0.0f),
    vec3(0.0f, 0.5f, 0.866025f),
    vec3(0.823639f, 0.5f, 0.267617f),
    vec3(0.509037f, 0.5f, -0.7006629f),
    vec3(-0.50937f, 0.5f, -0.7006629f),
    vec3(-0.823639f, 0.5f, 0.267617f)
};

const float diffuseConeWeights[] =
{
    PI / 4.0f,
    3.0f * PI / 20.0f,
    3.0f * PI / 20.0f,
    3.0f * PI / 20.0f,
    3.0f * PI / 20.0f,
    3.0f * PI / 20.0f,
};

vec4 AnisotropicSample(vec3 position, float mip, uvec3 faces, vec3 weight)
{
    float aniso_level = max(mip - 1.0, 0.0);
    vec4 sampled = weight.x * textureLod(voxels_directional_volumes[faces.x], position, aniso_level)
                 + weight.y * textureLod(voxels_directional_volumes[faces.y], position, aniso_level)
                 + weight.z * textureLod(voxels_directional_volumes[faces.z], position, aniso_level);

    if (mip < 1.0)
    {
        vec4 sampled0 = texture(voxels_radiance, position);
        sampled = mix(sampled0, sampled, clamp(mip, 0.0, 1.0));
    }


    return sampled;
}

// aperture = tan(teta / 2)
vec4 TraceCone(vec3 origin, vec3 direction, float aperture, float max_dist)
{
    uvec3 faces;
    faces.x = (direction.x < 0.0) ? 0 : 1;
    faces.y = (direction.y < 0.0) ? 2 : 3;
    faces.z = (direction.z < 0.0) ? 4 : 5;

    vec3 weight = direction * direction;

    vec3 p = origin;
    float d = voxel_options.size;

    float occlusion = 0.0;

    uint iter = 0;
    while (iter < 1000 && occlusion < 1.0f && d < max_dist)
    {
        p = origin + d * direction;

        float diameter = 2.0 * aperture * d;
        float mip_level = log2(diameter / voxel_options.size);
        vec3 voxel_pos = WorldToVoxelTex(p, voxel_options);

        vec4 sampled = AnisotropicSample(voxel_pos, mip_level, faces, weight);

        occlusion += ((1.0 - occlusion) * sampled.a) / ( 1.0 + d);

        d += diameter;
        iter += 1;
    }

    return vec4(vec3(1.0 - occlusion), 1);
}

vec4 AmbientOcclusion(vec3 normal)
{
    vec3 position = inWorldPos;
    vec4 visibility = vec4(0.0);

    // diffuse cone setup
    const float aperture = 0.57735f;
    vec3 guide = vec3(0.0f, 1.0f, 0.0f);

    if (abs(dot(normal,guide)) == 1.0f)
    {
        guide = vec3(0.0f, 0.0f, 1.0f);
    }

    // Find a tangent and a bitangent
    vec3 right = normalize(guide - dot(normal, guide) * normal);
    vec3 up = cross(right, normal);
    vec3 direction;

    for (uint i = 0; i < 6; i++)
    {
        direction = normal;
        direction += diffuseConeDirections[i].x * right + diffuseConeDirections[i].z * up;
        direction = normalize(direction);

        visibility += TraceCone(position, direction, aperture, 10.0) * diffuseConeWeights[i];
    }

    return visibility;
}

void main()
{
    vec3 normal = getNormal(inWorldPos, inNormal, normalTexture, inUV0);
    vec4 base_color = texture(baseColorTexture, inUV0);
    if (base_color.a < 0.5) {
        discard;
    }

    vec4 color = AmbientOcclusion(normal);

    if (debug.selected == 1)
    {
        color = vec4(base_color.rgb, 1);
    }
    else if (debug.selected == 2)
    {
        color = vec4(EncodeNormal(getNormal(inWorldPos, inNormal, normalTexture, inUV0)), 1);
    }
    else if (debug.selected == 3)
    {
        color = vec4(inWorldPos / 20, 1);
    }

    outColor = color;
}
