#pragma once
#include <exo/hash.h>
#include <exo/maths/numerics.h>
#include <string>

namespace exo
{
struct Serializer;
}

struct AssetId
{
	std::string name      = "";
	u64         name_hash = 0;

	template <typename T>
	static AssetId create(std::string_view name)
	{
		return AssetId{
			.name      = std::string(name),
			.name_hash = AssetId::hash_name(name),
		};
	}

	inline bool    operator==(const AssetId &other) const { return this->name_hash == other.name_hash; }
	inline bool    is_valid() const { return this->name_hash != 0; }
	static AssetId invalid() { return {}; }

private:
	static u64 hash_name(std::string_view name);
};

// hash
[[nodiscard]] u64 hash_value(const AssetId &id);

// Serialization
namespace exo
{
void serialize(exo::Serializer &serializer, AssetId &asset_id);
} // namespace exo
