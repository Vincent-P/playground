#pragma once
#include <exo/collections/index_map.h>
#include <exo/collections/vector.h>
#include <xxhash.h>

struct Asset;

using ConstructorFunc = Asset* (*)();

struct AssetConstructors
{
public:
    AssetConstructors()
    {
        indices_map = exo::IndexMap::with_capacity(64);
    }

    inline int add_constructor(const char *id, ConstructorFunc ctor)
    {
        const u64 hash = XXH3_64bits(id, strlen(id));
        indices_map.insert(hash, constructors.size());
        constructors.push_back(ctor);
        return 42;
    }

    inline Asset *create(std::string_view id)
    {
        const u64 hash = XXH3_64bits(id.data(), id.size());
        if (const auto index = indices_map.at(hash))
        {
            ConstructorFunc ctor = constructors[*index];
            return ctor();
        }
        return nullptr;
    }

private:
    exo::IndexMap indices_map;
    Vec<ConstructorFunc> constructors;
};
