#pragma once
#include <exo/maths/numerics.h>
#include <string>

namespace exo
{
struct Serializer;
}

using AssetTypeId = u64;

constexpr AssetTypeId create_asset_id(u32 val) { return (u64('BEAF') << 32) | u64(val); }

template <typename T> struct AssetTypeToAssetIdMap
{
	static const AssetTypeId id;
};

template <AssetTypeId ID> struct AssetIdToAssetTypeMap
{};

template <typename AssetType> consteval AssetTypeId get_asset_id() { return AssetTypeToAssetIdMap<AssetType>::id; }

template <AssetTypeId ID> consteval auto *get_asset_type()
{
	using AssetType = decltype(AssetIdToAssetTypeMap<ID>::ptr);
	return static_cast<AssetType>(nullptr);
}

#define REGISTER_ASSET_TYPE(_type, _id)                                                                                \
	struct _type;                                                                                                      \
	template <> const AssetTypeId AssetTypeToAssetIdMap<_type>::id = _id;                                              \
	template <> struct AssetIdToAssetTypeMap<_id>                                                                      \
	{                                                                                                                  \
		_type *ptr;                                                                                                    \
	};                                                                                                                 \
	static_assert(get_asset_id<_type>() == _id);                                                                       \
	static_assert(get_asset_type<_id>() == static_cast<_type *>(nullptr));

struct AssetId
{
	AssetTypeId type_id   = 0;
	std::string name      = "";
	u64         name_hash = 0;

	template <typename T> static AssetId create(std::string_view name)
	{
		return AssetId{
			.type_id   = get_asset_id<T>(),
			.name      = std::string(name),
			.name_hash = AssetId::hash_name(name),
		};
	}

	inline bool operator==(const AssetId &other) const
	{
		return this->type_id == other.type_id && this->name_hash == other.name_hash;
	}

	inline bool    is_valid() const { return this->type_id != 0 && this->name_hash != 0; }
	static AssetId invalid() { return {}; }

private:
	static u64 hash_name(std::string_view name);
};

u64 hash_value(const AssetId &id);

// Serialization
namespace exo
{
void serialize(exo::Serializer &serializer, AssetId &asset_id);
}
