#version 450

layout(set = 0, binding = 0, RGBA8) uniform image3D voxels_texture;

layout(set = 1, binding = 0) uniform UBO {
    vec4 position;
    vec4 front;
    vec4 up;
} cam;

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;

#define MAX_DIST 512
#define GRID_SIZE 256
#define EPSILON 0.1

float mincomp(vec3 v) {
    return min(min(v.x, v.y), v.z);
}

vec4 PlaneMarch(vec3 p0, vec3 d) {
    float t = 0;
    while (t < MAX_DIST) {
        vec3 p = p0 + d * t;
        vec4 c = imageLoad(voxels_texture, ivec3(floor(p)));
        if (c.a > 0) {
            return c;
        }

        vec3 deltas = (step(0, d) - fract(p)) / d;
        t += max(mincomp(deltas), EPSILON);
    }

    return vec4(0);
}


void main()
{
    vec3 p0 = cam.position.xyz;

    vec3 cam_right = cross(cam.front.xyz, cam.up.xyz);
    float x = inUV.x;
    float y = - inUV.y;
    vec3 d = cam.front.xyz + x * cam_right + y * cam.up.xyz;

    outColor = PlaneMarch(p0, d);
}
