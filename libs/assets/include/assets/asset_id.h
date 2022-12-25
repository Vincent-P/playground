#pragma once
#include "exo/hash.h"
#include "exo/maths/numerics.h"
#include "exo/string.h"
#include "exo/string_view.h"

namespace exo
{
struct Serializer;
}

struct AssetId
{
	exo::String name      = "";
	u64         name_hash = 0;

	template <typename T>
	static AssetId create(exo::StringView name)
	{
		return AssetId{
			.name      = exo::String(name),
			.name_hash = AssetId::hash_name(name),
		};
	}

	inline bool    operator==(const AssetId &other) const { return this->name_hash == other.name_hash; }
	inline bool    is_valid() const { return this->name_hash != 0; }
	static AssetId invalid() { return {}; }

private:
	static u64 hash_name(exo::StringView name);
};

// hash
[[nodiscard]] u64 hash_value(const AssetId &id);

// Serialization
namespace exo
{
void serialize(exo::Serializer &serializer, AssetId &asset_id);
} // namespace exo
