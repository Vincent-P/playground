#pragma once
#include <exo/collections/dynamic_array.h>
#include <exo/maths/u128.h>
#include <exo/path.h>

#include <reflection/reflection.h>

#include "assets/asset_database.h"
#include "assets/asset_id.h"
#include "assets/importers/importer.h"

namespace cross
{
struct FileWatcher;
struct Watch;
struct WatchEvent;
} // namespace cross
namespace exo
{
struct ScopeStack;
}

enum struct AssetErrors : int
{
	Invalid,
	NoImporterFound,
	NoLoaderFound,
	ParsingError,
	InvalidUUID,
};

struct AssetManager
{
	exo::DynamicArray<Importer *, 16> importers; // import resource into assets
	AssetDatabase                     database;

	// -- 

	static exo::Path     get_asset_path(AssetId id);
	static AssetManager *create(exo::ScopeStack &scope);

	template <typename T>
	T *create_asset(AssetId id)
	{
		const auto &type_info = refl::typeinfo<T>();
		void       *memory    = malloc(type_info.size);
		void       *asset_ptr = type_info.placement_ctor(memory);

		T *new_asset    = static_cast<T *>(asset_ptr);
		new_asset->uuid = id;
		this->database.insert_asset(refl::BasePtr<Asset>(new_asset));
		return new_asset;
	}

	template <typename T>
	T *load_asset_t(AssetId id)
	{
		refl::BasePtr<Asset> asset = this->database.get_asset(id);
		if (!asset.get()) {
			asset = this->_load_from_disk(id);
			this->database.insert_asset(asset);
		}
		auto *casted = asset.as<T>();
		ASSERT(casted != nullptr);
		return casted;
	}

	refl::BasePtr<Asset> load_asset(AssetId id)
	{
		auto asset = this->database.get_asset(id);
		ASSERT(asset.get());
		return asset;
	}

	void unload_asset(AssetId id);

	usize     read_blob(exo::u128 blob_hash, std::span<u8> out_data);
	exo::u128 save_blob(std::span<const u8> blob_data);

	refl::BasePtr<Asset> _load_from_disk(const AssetId &id);
	void   _save_to_disk(refl::BasePtr<Asset> asset);
	void   _import_resources(std::span<const Handle<Resource>> records);
};

struct ImporterApi
{
	// Used to create a new asset
	template <typename T>
	T *create_asset(AssetId id)
	{
		return manager.create_asset<T>(id);
	}
	// Used to retrieve an asset that was already processed
	template <typename T>
	T *retrieve_asset(AssetId id)
	{
		return manager.load_asset_t<T>(id);
	}

	inline exo::u128 save_blob(std::span<const u8> data) { return manager.save_blob(data); }

	AssetManager &manager;
};
