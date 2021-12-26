// -*- mode: glsl; -*-

#ifndef HASH_H
#define HASH_H

#include "base/types.h"

// https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
uint pcg(uint v)
{
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// http://www.jcgt.org/published/0009/03/02/
uvec3 pcg3d(uvec3 v)
{
    v = v * 1664525u + 1013904223u;

    v.x += v.y*v.z;
    v.y += v.z*v.x;
    v.z += v.x*v.y;

    v ^= v >> 16u;

    v.x += v.y*v.z;
    v.y += v.z*v.x;
    v.z += v.x*v.y;

    return v;
}

uvec3 hash3(uvec3 seed)
{
    return pcg3d(seed);
}

uvec3 hash2_3(uvec2 seed)
{
    uvec4 u = uvec4(seed, seed.x ^ seed.y, seed.x + seed.y);
    return hash3(u.xyz);
}

uvec3 hash1_3(uint seed)
{
    uvec2 u = uvec2(seed % 256, seed / 256);
    return hash2_3(u.xy);
}

float3 hash_to_float3(uvec3 hash)
{
    return float3(hash) * (1.0/float(0xffffffffu));
}

float2 hash_to_float2(uvec2 hash)
{
    return float2(hash) * (1.0/float(0xffffffffu));
}

float hash_to_float(uint hash)
{
    return hash * (1.0/float(0xffffffffu));
}

#endif
