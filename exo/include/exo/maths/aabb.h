#pragma once
#include "exo/prelude.h"

struct AABB
{
    float3 min = {0.0f};
    float3 max = {0.0f};
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
        if (new_point[i_comp] < aabb.min[i_comp]) {
            aabb.min[i_comp] = new_point[i_comp];
        }
        if (new_point[i_comp] > aabb.max[i_comp]) {
            aabb.max[i_comp] = new_point[i_comp];
        }
    }
}

inline void extend(AABB &aabb, const AABB &other)
{
    extend(aabb, other.min);
    extend(aabb, other.max);
}
