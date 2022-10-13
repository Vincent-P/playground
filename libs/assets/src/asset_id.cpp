#include "assets/asset_id.h"

#include <exo/hash.h>
#include <exo/serialization/serializer.h>

#include <xxhash.h>

u64 AssetId::hash_name(std::string_view name) { return XXH3_64bits(name.data(), name.size()); }

[[nodiscard]] u64 hash_value(const AssetId &id) { return exo::hash_combine(id.type_id, id.name_hash); }

namespace exo
{
void serialize(exo::Serializer &serializer, AssetId &asset_id)
{
	serialize(serializer, asset_id.type_id);

	const char *name = asset_id.name.c_str();
	serialize(serializer, name);
	if (!serializer.is_writing) {
		asset_id.name = std::string{name};
	}

	serialize(serializer, asset_id.name_hash);
}
} // namespace exo
