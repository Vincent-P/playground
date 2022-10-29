#pragma once
#include <assets/asset_database.h>
#include <exo/collections/map.h>
#include <exo/collections/pool.h>
#include <exo/collections/vector.h>
#include <exo/path.h>
#include <reflection/reflection.h>

#include <span>

#include "assets/asset_id.h"

namespace exo
{
struct Serializer;
}
namespace cross
{
struct JobManager;
}
struct Asset;

struct FileHash
{
	u64         hash                                            = 0;
	friend auto operator<=>(const FileHash &, const FileHash &) = default;
};

struct Resource
{
	AssetId   asset_id           = {};
	exo::Path resource_path      = {};
	FileHash  last_imported_hash = {};
};

[[nodiscard]] inline u64 hash_value(FileHash file_hash) { return file_hash.hash; }

// The asset database contains information about all assets (loaded or not) of a project
struct AssetDatabase
{
	static AssetDatabase create();

	exo::Pool<Resource>                   resource_records;
	exo::Map<exo::Path, Handle<Resource>> resource_path_map;
	exo::Map<FileHash, Handle<Resource>>  resource_content_map;

	exo::Map<AssetId, refl::BasePtr<Asset>> asset_id_map; // AssetId -> Asset*

	// Resources
	void      track_resource_changes(cross::JobManager &jobmanager, const exo::Path &directory, Vec<Handle<Resource>> &out_outdated_resources);
	Resource &get_resource_from_path(const exo::Path &path);
	Resource &get_resource_from_content(FileHash content_hash);

	// Assets
	refl::BasePtr<Asset> get_asset(const AssetId &id);
	void                 insert_asset(refl::BasePtr<Asset> asset);
};

void serialize(exo::Serializer &serializer, Resource &data);
void serialize(exo::Serializer &serializer, AssetDatabase &db);
void serialize(exo::Serializer &serializer, FileHash &hash);
