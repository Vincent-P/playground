#pragma once
#include <exo/collections/index_map.h>
#include <exo/collections/pool.h>
#include <exo/collections/vector.h>
#include <exo/path.h>

#include <span>

#include "assets/asset_id.h"

namespace exo
{
struct Serializer;
}
struct Asset;

struct Resource
{
	AssetId   asset_id           = {};
	exo::Path resource_path      = {};
	u64       last_imported_hash = 0;
};

// The asset database contains information about all assets (loaded or not) of a project
struct AssetDatabase
{
	static AssetDatabase create();

	exo::Pool<Resource> resource_records;
	exo::IndexMap       resource_path_map;    // <path> -> Handle<ResourceRecord>
	exo::IndexMap       resource_content_map; // <content_hash> -> Handle<ResourceRecord>

	exo::IndexMap asset_id_map; // AssetId -> Asset*

	// Resources
	void      track_resource_changes(const exo::Path &directory, Vec<Handle<Resource>> &out_outdated_resources);
	Resource &get_resource_from_path(const exo::Path &path);
	Resource &get_resource_from_content(u64 content_hash);

	// Assets
	Asset *get_asset(AssetId);
	void   insert_asset(Asset *asset);
};

void serialize(exo::Serializer &serializer, Resource &data);
void serialize(exo::Serializer &serializer, AssetDatabase &db);
