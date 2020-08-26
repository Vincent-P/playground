#include "voxels.h"

layout(set = 1, binding = 0) uniform VO {
    VoxelOptions voxel_options;
};

layout(set = 1, binding = 1) uniform UBO {
    vec4 position;
    vec4 front;
    vec4 up;
    float opacity;
} cam;

layout (set = 1, binding = 2) uniform UBODebug {
    uint selected;
    float opacity;
} debug;


layout(set = 1, binding = 3, rgba16) uniform image3D voxels_albedo;
layout(set = 1, binding = 4, rgba16) uniform image3D voxels_normal;
layout(set = 1, binding = 5, rgba8) uniform image3D voxels_radiance;


layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;

#define MAX_DIST (voxel_options.res * 2)
#define EPSILON 0.001

float mincomp(vec3 v) {
    return min(min(v.x, v.y), v.z);
}

/// vec4 PlaneMarch(uimage3D voxels, vec3 p0, vec3 d)
#define PlaneMarch(ret, voxels, p0, d)                              \
    {                                                                   \
        float t = 0;                                                    \
        while (t < MAX_DIST) {                                          \
            vec3 p = p0 + d * t;                                        \
            ivec3 voxel_pos = ivec3(floor(p));                          \
            vec4 voxel = imageLoad(voxels, voxel_pos);              \
            if (voxel.a > EPSILON)                                      \
            {                                                           \
                ret = vec4(voxel.rgb, debug.opacity);\
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
    float x = 2 * inUV.x - 1.0;
    float y = - (2 * inUV.y - 1.0) * 9 / 16;
    vec3 d = normalize(cam.front.xyz + x * cam_right + y * normalize(cam.up.xyz));

    vec4 color = vec4(0);
    if (debug.selected == 1) {
        PlaneMarch(color, voxels_albedo, p0, d);
    }
    else if (debug.selected == 2) {
        PlaneMarch(color, voxels_normal, p0, d);
        color = normalize(color);
    }
    else if (debug.selected == 3) {
        PlaneMarch(color, voxels_radiance, p0, d);
    }
    if (color.a == 0) {
        discard;
    }
    outColor = color;
}
