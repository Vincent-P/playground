#ifndef MATHS_H
#define MATHS_H

#include "types.h"
#include "constants.h"

float max2(vec2 values) {
    return max(values.x, values.y);
}

float max3(vec3 values) {
    return max(values.x, max(values.y, values.z));
}

float max4(vec4 values) {
    return max(max(values.x, values.y), max(values.z, values.w));
}

float min2(vec2 values) {
    return min(values.x, values.y);
}

float min3(vec3 values) {
    return min(values.x, min(values.y, values.z));
}

float min4(vec4 values) {
    return min(min(values.x, values.y), min(values.z, values.w));
}

float safe_dot(float3 a, float3 b)
{
    return max(dot(a, b), 0.0);
}

void make_orthogonal_coordinate_system(float3 v1, out float3 v2, out float3 v3)
{
    if (abs(v1.x) > abs(v1.y))
    {
        v2 = float3(-v1.z, 0, v1.x) * (1.0 / sqrt(v1.x * v1.x + v1.z * v1.z));
    }
    else
    {
        v2 = float3(0, v1.z, -v1.y) * (1.0 / sqrt(v1.y * v1.y + v1.z * v1.z));
    }
    v3 = cross(v1, v2);
}

// RNG

uint init_seed(uint2 pixel_pos, uint frame_count)
{
    return uint(uint(pixel_pos.x) * uint(1973) + uint(pixel_pos.y) * uint(9277) + uint(frame_count) * uint(26699)) | uint(1);
}

uint wang_hash(inout uint seed)
{
    seed = uint(seed ^ uint(61)) ^ uint(seed >> uint(16));
    seed *= uint(9);
    seed = seed ^ (seed >> 4);
    seed *= uint(0x27d4eb2d);
    seed = seed ^ (seed >> 15);
    return seed;
}

float random_float_01(inout uint seed)
{
    return float(wang_hash(seed)) / 4294967296.0;
}

float3 random_unit_vector(inout uint rng_seed)
{
    float z = random_float_01(rng_seed) * 2.0 - 1.0;
    float a = random_float_01(rng_seed) * 2.0 * PI;
    float r = sqrt(1.0f - z * z);
    float x = r * cos(a);
    float y = r * sin(a);
    return float3(x, y, z);
}

#endif
