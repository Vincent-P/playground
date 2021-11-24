#pragma once
#include <cross/uuid.h>
#include <exo/collections/vector.h>
#include <exo/maths/vectors.h>

#include "assets/asset.h"
#include "schemas/exo_generated.h"
#include "schemas/asset_generated.h"

// -- flatbuffers types -> C++ types

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



// -- C++ types -> flatbuffer types

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



// -- Asset base functions

inline void from_asset(const engine::schemas::Asset* fb_asset, Asset *out_asset)
{
    auto new_uuid = from(fb_asset->uuid());
    ASSERT(out_asset->uuid == new_uuid);

    out_asset->dependencies = from<cross::UUID>(fb_asset->dependencies());
}

inline flatbuffers::Offset<engine::schemas::Asset> to_asset(const Asset *in_asset, flatbuffers::FlatBufferBuilder &builder)
{
    Vec<engine::schemas::exo::UUID> dependencies_uuid = to<engine::schemas::exo::UUID>(in_asset->dependencies);
    auto dependencies_offset = builder.CreateVectorOfStructs(dependencies_uuid);

    engine::schemas::AssetBuilder asset_builder{builder};
    asset_builder.add_uuid(to(in_asset->uuid));
    asset_builder.add_dependencies(dependencies_offset);

    return asset_builder.Finish();
}
