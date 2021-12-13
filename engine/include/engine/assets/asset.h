#pragma once

#include <exo/collections/vector.h>
#include <exo/collections/enum_array.h>
#include <exo/os/uuid.h>
#include <exo/serializer.h>
#include <string>

enum struct AssetState
{
    Unloaded,
    Loading,
    Loaded,
    Count
};

inline constexpr exo::EnumArray<const char *, AssetState> asset_state_to_string{
    "Unloaded",
    "Loading",
    "Loaded",
};

inline constexpr const char *to_string(AssetState state) { return asset_state_to_string[state]; }

struct Asset
{
    virtual ~Asset() {}

    virtual const char *type_name() const = 0;
    virtual void serialize(exo::Serializer& serializer) = 0;
    virtual void display_ui() = 0;

    bool operator==(const Asset &other) const = default;

    inline void add_dependency_checked(exo::UUID dependency)
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

    // --
    exo::UUID uuid;
    AssetState state;
    const char *name;
    Vec<exo::UUID> dependencies;
};

// https://isocpp.org/wiki/faq/ctors#static-init-order-on-first-use
struct AssetConstructors &global_asset_constructors();

template<>
void exo::Serializer::serialize<Asset>(Asset &data);
