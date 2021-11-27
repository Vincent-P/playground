#pragma once

#include <exo/collections/vector.h>
#include <exo/collections/enum_array.h>
#include <exo/cross/uuid.h>
#include <string>


namespace flatbuffers {class FlatBufferBuilder; }

enum struct AssetState
{
    Unloaded,
    Loading,
    Loaded,
    Count
};

inline constexpr EnumArray<const char *, AssetState> asset_state_to_string{
    "Unloaded",
    "Loading",
    "Loaded",
};

inline constexpr const char *to_string(AssetState state) { return asset_state_to_string[state]; }

struct Asset
{
    cross::UUID uuid;
    AssetState state;
    std::string name;

    Vec<cross::UUID> dependencies;

    inline void add_dependency_checked(cross::UUID dependency)
    {
        usize i = 0;
        for (; i < dependencies.size(); i += 1)
        {
            if (dependencies[i] == dependency)
            {
                break;
            }
        }
        if (i >= dependencies.size())
        {
            dependencies.push_back(dependency);
        }
    }

    virtual ~Asset() {}

    virtual const char *type_name() const { return "Asset"; }

    virtual void from_flatbuffer(const void *data, usize len) = 0;
    virtual void to_flatbuffer(flatbuffers::FlatBufferBuilder &builder, u32 &o_offset, u32 &o_size) const = 0;

    virtual void display_ui() {};

    bool operator==(const Asset &other) const = default;
};
