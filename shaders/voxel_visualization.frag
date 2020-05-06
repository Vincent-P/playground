#version 450

layout(set = 1, binding = 0, r32ui) uniform uimage3D voxels_texture;

layout(set = 1, binding = 1) uniform UBO {
    vec4 position;
    vec4 front;
    vec4 up;
} cam;

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;

#define GRID_SIZE 512
#define MAX_DIST GRID_SIZE
#define EPSILON 0.1

float mincomp(vec3 v) {
    return min(min(v.x, v.y), v.z);
}

vec3 PlaneMarch(vec3 p0, vec3 d) {
    float t = 0;
    while (t < MAX_DIST) {
        vec3 p = p0 + d * t;
        uint voxel = imageLoad(voxels_texture, ivec3(floor(p))).r;
        if (voxel != 0)
        {
            vec4 c = unpackUnorm4x8(voxel);
            return abs(c.xyz);
        }

        vec3 deltas = (step(0, d) - fract(p)) / d;
        t += max(mincomp(deltas), EPSILON);
    }

    discard;
    return vec3(1);
}


void main()
{
    vec3 p0 = cam.position.xyz;

    vec3 cam_right = cross(cam.front.xyz, cam.up.xyz);
    float x = inUV.x;
    float y = - inUV.y;
    vec3 d = cam.front.xyz + x * cam_right + y * cam.up.xyz;

    outColor = vec4(PlaneMarch(p0, d), 1);
}
