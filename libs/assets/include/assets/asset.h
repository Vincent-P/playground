#pragma once

#include "exo/collections/enum_array.h"
#include "exo/collections/vector.h"
#include "exo/uuid.h"

#include "reflection/reflection.h"

#include "assets/asset_id.h"

#include "exo/string.h"

namespace exo
{
struct Serializer;
}

enum struct AssetState
{
	// The asset has been deserialized, but its dependencies may not exsit or be loaded yet
	LoadedWaitingForDeps,
	FullyLoaded,
	Installed,
	Count
};

inline constexpr exo::EnumArray<const char *, AssetState> asset_state_to_string{
	"Waiting for dependencies",
	"Loaded",
	"Installed",
};

inline constexpr const char *to_string(AssetState state) { return asset_state_to_string[state]; }

struct Asset
{
	using Self = Asset;
	REFL_REGISTER_TYPE("Asset")

	AssetId      uuid;
	AssetState   state;
	exo::String  name;
	exo::String  path;
	Vec<AssetId> dependencies;

	// --
	virtual ~Asset() {}

	virtual void serialize(exo::Serializer &serializer) = 0;

	bool operator==(const Asset &other) const = default;

	inline void add_dependency_checked(AssetId dependency)
	{
		usize i = 0;
		for (; i < dependencies.len(); i += 1) {
			if (dependencies[i] == dependency) {
				break;
			}
		}
		if (i >= dependencies.len()) {
			dependencies.push(std::move(dependency));
		}
	}
};
