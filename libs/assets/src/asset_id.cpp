#include "assets/asset_id.h"

#include <exo/hash.h>
#include <exo/serialization/serializer.h>
#include <exo/serialization/string_serializer.h>
#include "exo/string_view.h"

#include <xxhash.h>

u64 AssetId::hash_name(exo::StringView name) { return XXH3_64bits(name.data(), name.size()); }

[[nodiscard]] u64 hash_value(const AssetId &id) { return id.name_hash; }

namespace exo
{
void serialize(exo::Serializer &serializer, AssetId &asset_id)
{
	serialize(serializer, asset_id.name);
	serialize(serializer, asset_id.name_hash);
}
} // namespace exo
