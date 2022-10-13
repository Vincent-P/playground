#pragma once
#include "assets/asset_id.h"
#include <exo/collections/map.h>

struct Asset;

using ConstructorFunc = Asset *(*)();

struct AssetConstructors
{
public:
	inline int add_constructor(AssetTypeId type_id, ConstructorFunc ctor)
	{
		indices_map.insert(type_id, ctor);
		return 42;
	}

	inline Asset *create(AssetTypeId type_id)
	{
		if (auto *ctor = indices_map.at(type_id)) {
			return (*ctor)();
		}
		return nullptr;
	}

private:
	exo::Map<AssetTypeId, ConstructorFunc> indices_map = {};
};
