// -*- mode: glsl; -*-

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

// rng contains two random floats between 0 and 1
float3 sample_hemisphere(float2 rng)
{
    float z = rng[0];
    float r = sqrt(max(0.0f, 1.0f - z * z));
    float phi = 2 * PI * rng[1];
    float x = r * cos(phi);
    float y = r * sin(phi);
    return float3(x, y, z);
}

float3 sample_cosine_weighted_hemisphere(float2 rng)
{
    float z = sqrt(max(0.0f, 1.0f - rng[0]));
    float r = sqrt(rng[0]);
    float phi = 2 * PI * rng[1];
    float x = r * cos(phi);
    float y = r * sin(phi);
    return float3(x, y, z);
}

float3 sample_cosine_weighted_hemisphere_from_disk(float2 disk)
{
    float r2 = dot(disk, disk);
    float z  = sqrt(max(0.0, 1.0 - r2));
    return float3(disk.x, disk.y, z);
}

float3 random_unit_vector(float2 rng)
{
    float z = rng[0] * 2.0 - 1.0;
    float a = rng[1] * 2.0 * PI;
    float r = sqrt(1.0f - z * z);
    float x = r * cos(a);
    float y = r * sin(a);
    return float3(x, y, z);
}

#endif
