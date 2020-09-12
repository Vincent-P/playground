#include "voxels.h"
#include "pbr.h"

#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;

layout(set = 1, binding = 1) uniform VO {
    VoxelOptions voxel_options;
};

layout(set = 1, binding = 3, r32ui) uniform uimage3D voxels_albedo;
layout(set = 1, binding = 4, r32ui) uniform uimage3D voxels_normal;

void main()
{
    vec4 color = texture(global_textures[constants.base_color_idx], inUV0);
    if (color.a < 0.1) {
        discard;
    }
    vec3 normal = vec3(0.0);
    getNormalM(normal, inWorldPos, inNormal, global_textures[constants.normal_map_idx], inUV0);

    // output:
    ivec3 voxel_pos = WorldToVoxel(inWorldPos, voxel_options);

    imageAtomicAverageRGBA8(voxels_albedo, voxel_pos, color.rgb);
    imageAtomicAverageRGBA8(voxels_normal, voxel_pos, EncodeNormal(normal));
}
