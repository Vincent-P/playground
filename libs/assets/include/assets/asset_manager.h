#pragma once
#include "assets/asset.h"
#include "exo/collections/dynamic_array.h"
#include "exo/maths/u128.h"
#include "exo/path.h"
#include "exo/profile.h"

#include "reflection/reflection.h"

#include "assets/asset_database.h"
#include "assets/asset_id.h"
#include "assets/importers/importer.h"

namespace cross
{
struct FileWatcher;
struct Watch;
struct WatchEvent;
struct JobManager;
} // namespace cross

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
	cross::JobManager                *jobmanager;

	// --

	static exo::Path    get_asset_path(const AssetId &id);
	static AssetManager create(cross::JobManager &jobmanager);

	template <typename T>
	T *load_asset_t(AssetId id)
	{
		refl::BasePtr<Asset> asset  = this->database.get_asset(id);
		auto                *casted = asset.as<T>();
		ASSERT(casted != nullptr);
		return casted;
	}

	refl::BasePtr<Asset> load_asset(AssetId id)
	{
		auto asset = this->database.get_asset(id);
		return asset;
	}

	void unload_asset(const AssetId &id);

	// -- Async loading
	bool is_loaded(const AssetId &id)
	{
		auto asset = this->database.get_asset(id);
		return asset.is_valid() &&
		       (asset->state == AssetState::FullyLoaded || asset->state == AssetState::LoadedWaitingForDeps);
	}

	bool is_fully_loaded(const AssetId &id)
	{
		auto asset = this->database.get_asset(id);
		return asset.is_valid() && asset->state == AssetState::FullyLoaded;
	}

	// Request an asset, needs to poll `is_loaded` or `is_fully_loaded` to check if request has been processed
	void load_asset_async(const AssetId &id);
	// Called one time per frame to poll for async requests status
	void update_async();
	// Called by `update_async` when a load request has been processed
	void finish_loading_async(refl::BasePtr<Asset> asset);

	// -- Binary blobs
	// Binary data in assets is serialized as 'blobs' and is addresed using content hash
	usize     read_blob(exo::u128 blob_hash, exo::Span<u8> out_data);
	exo::u128 save_blob(exo::Span<const u8> blob_data);

	static refl::BasePtr<Asset> _load_from_disk(const AssetId &id);
	void                        _save_to_disk(refl::BasePtr<Asset> asset);
	void                        _import_resources(exo::Span<const Handle<Resource>> records);
	void                        _load_deps_async_if_needed(const AssetId &id);
};

struct ImporterApi
{
	AssetManager &manager;

	// --

	// Used to create a new asset in place
	template <typename T>
	T *create_asset(AssetId id)
	{
		const auto &type_info = refl::typeinfo<T>();
		void       *memory    = malloc(type_info.size);
		EXO_PROFILE_MALLOC(memory, type_info.size);
		void *asset_ptr = type_info.placement_ctor(memory);

		T *new_asset    = static_cast<T *>(asset_ptr);
		new_asset->uuid = id;
		manager.database.insert_asset(refl::BasePtr<Asset>(new_asset));
		return new_asset;
	}

	// Used to retrieve an asset that was already processed
	template <typename T>
	T *retrieve_asset(AssetId id)
	{
		return manager.load_asset_t<T>(id);
	}

	inline exo::u128 save_blob(exo::Span<const u8> data) { return manager.save_blob(data); }
};
