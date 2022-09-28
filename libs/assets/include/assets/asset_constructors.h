#pragma once
#include <exo/collections/index_map.h>
#include <exo/collections/vector.h>
#include <xxhash.h>

#include "assets/asset_id.h"

struct Asset;

using ConstructorFunc = Asset *(*)();

struct AssetConstructors
{
public:
	AssetConstructors() { indices_map = exo::IndexMap::with_capacity(64); }

	inline int add_constructor(AssetTypeId type_id, ConstructorFunc ctor)
	{
		indices_map.insert(type_id, constructors.size());
		constructors.push_back(ctor);
		return 42;
	}

	inline Asset *create(AssetTypeId type_id)
	{
		if (const auto index = indices_map.at(type_id)) {
			ConstructorFunc ctor = constructors[*index];
			return ctor();
		}
		return nullptr;
	}

private:
	exo::IndexMap        indices_map;
	Vec<ConstructorFunc> constructors;
};
