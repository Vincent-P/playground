#ifndef HASH_H
#define HASH_H

#include "types.h"

// http://www.jcgt.org/published/0009/03/02/
uvec3 pcg3d(uvec3 v) {

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

uvec3 hash(vec2 s)
{
    uvec4 u = uvec4(s, uint(s.x) ^ uint(s.y), uint(s.x) + uint(s.y));
    return pcg3d(u.xyz);
}

float3 hash_color(uvec3 hash)
{
    return float3(hash) * (1.0/float(0xffffffffu));
}


#endif
