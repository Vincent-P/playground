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

vec4 PlaneMarch(vec3 p0, vec3 d) {
    float t = 0;
    while (t < MAX_DIST) {
        vec3 p = p0 + d * t;
        uint voxel = imageLoad(voxels_texture, ivec3(floor(p))).r;
        if (voxel != 0)
        {
            return vec4(abs(unpackUnorm4x8(voxel)).xyz, 1);
        }

        vec3 deltas = (step(0, d) - fract(p)) / d;
        t += max(mincomp(deltas), EPSILON);
    }

    return vec4(0);
}


void main()
{
    vec3 p0 = cam.position.xyz;

    vec3 cam_right = cross(normalize(cam.front.xyz), normalize(cam.up.xyz));
    float x = inUV.x;
    float y = - inUV.y * 9 / 16;
    vec3 d = cam.front.xyz + x * cam_right + y * normalize(cam.up.xyz);

    vec4 color = PlaneMarch(p0, d);
    if (color.a == 0) {
        discard;
    }
    outColor = color;
}
