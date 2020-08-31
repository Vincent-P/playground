#include "voxels.h"
#include "globals.h"
#include "types.h"

layout(set = 1, binding = 0) uniform VO {
    VoxelOptions voxel_options;
};

layout(set = 1, binding = 1) uniform UBO {
    vec4 position;
    vec4 front;
    vec4 up;
} cam;

layout (set = 1, binding = 2) uniform UBODebug {
    uint selected;
} debug;


layout(set = 1, binding = 3, rgba16) uniform image3D voxels_albedo;
layout(set = 1, binding = 4, rgba16) uniform image3D voxels_normal;
layout(set = 1, binding = 5, rgba8) uniform image3D voxels_radiance;


layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;

#define MAX_DIST (voxel_options.res * 2)
#define EPSILON 0.001

void main()
{
    float2 pixel_pos = gl_FragCoord.xy;
    float2 uv = pixel_pos / global.resolution;

    float3 clip_space = float3(uv * 2.0 - float2(1.0), 0.001);
    float4 h_pos      = global.camera_inv_view_proj * float4(clip_space, 1.0);
    h_pos /= h_pos.w;

    float3 rayDir = normalize(h_pos.xyz - global.camera_pos);
    float3 rayPos = WorldToVoxel(global.camera_pos, voxel_options);


    ivec3 mapPos = ivec3(floor(rayPos + 0.));
    vec3 deltaDist = abs(vec3(length(rayDir)) / rayDir);
    ivec3 rayStep = ivec3(sign(rayDir));
    vec3 sideDist = (sign(rayDir) * (vec3(mapPos) - rayPos) + (sign(rayDir) * 0.5) + 0.5) * deltaDist;

    bvec3 mask;

    int i = 0;
    float4 voxel = float4(0.0);
    for (i = 0; i < MAX_DIST; i++) {
        if (debug.selected == 0) {
            voxel = imageLoad(voxels_albedo, mapPos);
        }
        else if (debug.selected == 1) {
            voxel = imageLoad(voxels_normal, mapPos);
        }
        else if (debug.selected == 2) {
            voxel = imageLoad(voxels_radiance, mapPos);
        }
        if (voxel.a > EPSILON) break;
        if (sideDist.x < sideDist.y) {
            if (sideDist.x < sideDist.z) {
                sideDist.x += deltaDist.x;
                mapPos.x += rayStep.x;
                mask = bvec3(true, false, false);
            }
            else {
                sideDist.z += deltaDist.z;
                mapPos.z += rayStep.z;
                mask = bvec3(false, false, true);
            }
        }
        else {
            if (sideDist.y < sideDist.z) {
                sideDist.y += deltaDist.y;
                mapPos.y += rayStep.y;
                mask = bvec3(false, true, false);
            }
            else {
                sideDist.z += deltaDist.z;
                mapPos.z += rayStep.z;
                mask = bvec3(false, false, true);
            }
        }
    }

    if (voxel.a > EPSILON)
    {
        outColor = float4(voxel.xyz, 1.0);
    }
    else
    {
        outColor = float4(0.0);
    }
}
