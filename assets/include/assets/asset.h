#pragma once

#include <exo/collections/enum_array.h>
#include <exo/collections/vector.h>
#include <exo/uuid.h>

#include <string>

#include "assets/asset_id.h"

namespace exo
{
struct Serializer;
}

enum struct AssetState
{
	Unloaded,
	Loading,
	Loaded,
	Installed,
	Count
};

inline constexpr exo::EnumArray<const char *, AssetState> asset_state_to_string{
	"Unloaded",
	"Loading",
	"Loaded",
	"Installed",
};

inline constexpr const char *to_string(AssetState state) { return asset_state_to_string[state]; }

struct Asset
{
	virtual ~Asset() {}

	virtual const char *type_name() const                      = 0;
	virtual void        serialize(exo::Serializer &serializer) = 0;

	bool operator==(const Asset &other) const = default;

	inline void add_dependency_checked(AssetId dependency)
	{
		usize i = 0;
		for (; i < dependencies.size(); i += 1) {
			if (dependencies[i] == dependency) {
				break;
			}
		}
		if (i >= dependencies.size()) {
			dependencies.push_back(std::move(dependency));
		}
	}

	// --
	AssetId      uuid;
	AssetState   state;
	std::string  name;
	std::string  path;
	Vec<AssetId> dependencies;
};

// https://isocpp.org/wiki/faq/ctors#static-init-order-on-first-use
struct AssetConstructors &global_asset_constructors();
