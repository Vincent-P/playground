#pragma once
#include "exo/vectors.h"

struct AABB
{
    float3 min;
    float3 max;
};

inline float3 center(const AABB &aabb)
{
    return (aabb.min + aabb.max) * 0.5f;
}

inline float3 extent(const AABB &aabb)
{
    return (aabb.max - aabb.min);
}

inline void extend(AABB &aabb, float3 new_point)
{
    for (uint i_comp = 0; i_comp < 3; i_comp += 1)
    {
        if (new_point.raw[i_comp] < aabb.min.raw[i_comp]) {
            aabb.min.raw[i_comp] = new_point.raw[i_comp];
        }
        if (new_point.raw[i_comp] > aabb.max.raw[i_comp]) {
            aabb.max.raw[i_comp] = new_point.raw[i_comp];
        }
    }
}

inline void extend(AABB &aabb, const AABB &other)
{
    extend(aabb, other.min);
    extend(aabb, other.max);
}
