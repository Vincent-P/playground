#version 460
#include "voxels.h"
#include "pbr.h"

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;
layout (location = 4) in vec3 inVoxelPos;

layout(set = 1, binding = 0) uniform VO {
    VoxelOptions voxel_options;
};

layout(set = 1, binding = 2, r32ui) uniform uimage3D voxels_albedo;
layout(set = 1, binding = 3, r32ui) uniform uimage3D voxels_normal;

layout(set = 2, binding = 1) uniform sampler2D baseColorTexture;
layout(set = 2, binding = 2) uniform sampler2D normalTexture;

layout (push_constant) uniform MU {
    MaterialUniform material;
};

void main()
{
    vec4 color = texture(baseColorTexture, inUV0);
    if (color.a < 0.1) {
        discard;
    }
    vec3 normal = getNormal(inWorldPos, inNormal, normalTexture, inUV0);

    // output:
    ivec3 voxel_pos = ivec3(inVoxelPos);
    imageAtomicAverageRGBA8(voxels_albedo, voxel_pos, color.rgb);
    imageAtomicAverageRGBA8(voxels_normal, voxel_pos, normal);
}
