#version 450

#include "voxels.h"

layout(set = 1, binding = 0) uniform VO {
    VoxelOptions voxel_options;
};

layout(set = 1, binding = 1) uniform UBO {
    vec4 position;
    vec4 front;
    vec4 up;
} cam;

layout(set = 1, binding = 2, r32ui) uniform uimage3D voxels_albedo;
layout(set = 1, binding = 3, r32ui) uniform uimage3D voxels_normal;


layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;

#define MAX_DIST (voxel_options.res * 2)
#define EPSILON 0.001

float mincomp(vec3 v) {
    return min(min(v.x, v.y), v.z);
}

/// vec4 PlaneMarch(uimage3D voxels, vec3 p0, vec3 d)
#define PlaneMarch(ret, voxels, p0, d)                                  \
    {                                                                   \
        float t = 0;                                                    \
        while (t < MAX_DIST) {                                          \
            vec3 p = p0 + d * t;                                        \
            ivec3 voxel_pos = ivec3(floor(p));                          \
            uint voxel = imageLoad(voxels, voxel_pos).r;                \
            if (voxel != 0)                                             \
            {                                                           \
                ret = vec4(abs(unpackUnorm4x8(voxel)).xyz, 0.5);        \
                break;                                                  \
            }                                                           \
                                                                        \
            vec3 deltas = (step(0, d) - fract(p)) / d;                  \
            t += max(mincomp(deltas), EPSILON);                         \
        }                                                               \
    }


void main()
{
    vec3 p0 = (cam.position.xyz - voxel_options.center) / (voxel_options.size);

    vec3 cam_right = cross(normalize(cam.front.xyz), normalize(cam.up.xyz));
    float x = inUV.x;
    float y = - inUV.y * 9 / 16;
    vec3 d = normalize(cam.front.xyz + x * cam_right + y * normalize(cam.up.xyz));

    vec4 color = vec4(0);
    PlaneMarch(color, voxels_albedo, p0, d);
    if (color.a == 0) {
        discard;
    }
    outColor = color;
}
