#pragma once
#include <cross/uuid.h>
#include <exo/collections/vector.h>
#include <exo/maths/vectors.h>

#include "schemas/exo_generated.h"

inline float4 from(const engine::schemas::exo::float4 *fb_float4)
{
    return *reinterpret_cast<const float4*>(fb_float4);
}

inline cross::UUID from(const engine::schemas::exo::UUID *fb_uuid)
{
    ASSERT(fb_uuid->v()->size() == 4);
    u32 values[4] = {};
    for (u32 i_value = 0; i_value < 4; i_value += 1)
    {
        values[i_value] = fb_uuid->v()->Get(i_value);
    }
    return cross::UUID::from_values(values);
}

template<typename T>
inline Vec<T> from(const auto *fb_vector)
{
    Vec<T> out_vector;
    out_vector.reserve(fb_vector->size());
    for (const auto *element : *fb_vector)
    {
        out_vector.push_back(from(element));
    }
    return out_vector;
}

inline const engine::schemas::exo::UUID* to(const cross::UUID &uuid)
{
    return reinterpret_cast<const engine::schemas::exo::UUID*>(&uuid);
}

inline const engine::schemas::exo::float4* to(const float4 &v)
{
    return reinterpret_cast<const engine::schemas::exo::float4*>(&v);
}

template<typename T>
inline Vec<T> to(const auto &in_vector)
{
    Vec<T> fb_uuids;
    fb_uuids.reserve(in_vector.size());
    for (const auto &element : in_vector)
    {
        fb_uuids.push_back(*to(element));
    }
    return fb_uuids;
}
