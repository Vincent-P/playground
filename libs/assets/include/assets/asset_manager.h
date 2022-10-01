#pragma once
#include <exo/collections/dynamic_array.h>
#include <exo/maths/u128.h>
#include <exo/path.h>

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

	static exo::Path     get_asset_path(AssetId id);
	static AssetManager *create(exo::ScopeStack &scope);

	template <typename T> T *create_asset(AssetId id)
	{
		Asset *new_asset = T::create();
		new_asset->uuid  = id;
		this->database.insert_asset(new_asset);
		return static_cast<T *>(new_asset);
	}

	template <typename T> T *load_asset(AssetId id)
	{
		Asset *asset = this->database.get_asset(id);
		if (!asset) {
			asset = this->load_from_disk(id);
			this->database.insert_asset(asset);
		}
		auto *casted = dynamic_cast<T *>(asset);
		ASSERT(casted != nullptr);
		return casted;
	}

	template <> Asset *load_asset(AssetId id)
	{
		Asset *asset = this->database.get_asset(id);
		ASSERT(asset);
		return asset;
	}

	void unload_asset(AssetId id);

	usize     read_blob(exo::u128 blob_hash, std::span<u8> out_data);
	exo::u128 save_blob(std::span<const u8> blob_data);

private:
	Asset *load_from_disk(const AssetId &id);
	void   import_resources(std::span<const Handle<Resource>> records);
};

struct ImporterApi
{
	// Used to create a new asset
	template <typename T> T *create_asset(AssetId id) { return manager.create_asset<T>(id); }
	// Used to retrieve an asset that was already processed
	template <typename T> T *retrieve_asset(AssetId id) { return manager.load_asset<T>(id); }

	inline exo::u128 save_blob(std::span<const u8> data) { return manager.save_blob(data); }

	AssetManager &manager;
};
