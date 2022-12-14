#pragma once
#include "assets/asset_database.h"
#include "assets/asset_id.h"
#include "cross/jobs/waitable.h"
#include "exo/collections/map.h"
#include "exo/collections/pool.h"
#include "exo/collections/set.h"
#include "exo/collections/span.h"
#include "exo/hash.h"
#include "exo/path.h"
#include "reflection/reflection.h"
#include <memory>

namespace exo
{
struct Serializer;
}
namespace cross
{
struct JobManager;
} // namespace cross
struct Asset;
struct AssetManager;

struct Resource
{
	AssetId asset_id = {};
	exo::Path resource_path = {};
	exo::RawHash last_imported_hash = {};
};
void serialize(exo::Serializer &serializer, Resource &data);

struct AssetAsyncRequest
{
	struct Data
	{
		AssetId asset_id = {};
		refl::BasePtr<Asset> result = {};
	};
	std::unique_ptr<Data> data = {};
	std::unique_ptr<cross::Waitable> waitable = {};
};

// The asset database contains information about all assets (loaded or not) of a project
struct AssetDatabase
{
	// Persistent database of known resources
	exo::Pool<Resource> resource_records;
	exo::Map<exo::Path, Handle<Resource>> resource_path_map;
	exo::Map<exo::RawHash, Handle<Resource>> resource_content_map;

	// Runtime map containing loaded assets
	exo::Map<AssetId, refl::BasePtr<Asset>> asset_id_map;

	// Async loading
	exo::Map<AssetId, AssetAsyncRequest> asset_async_requests;
	exo::Set<AssetId> asset_async_waiting_for_deps;

	// --
	// Resources
	void track_resource_changes(
		cross::JobManager &jobmanager, const exo::Path &directory, Vec<Handle<Resource>> &out_outdated_resources);
	Resource &get_resource_from_path(const exo::Path &path);
	Resource &get_resource_from_content(exo::RawHash content_hash);

	// Assets
	refl::BasePtr<Asset> get_asset(const AssetId &id);
	void remove_asset(const AssetId &id);
	void insert_asset(refl::BasePtr<Asset> asset);
};
void serialize(exo::Serializer &serializer, AssetDatabase &db);
