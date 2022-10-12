#include "assets/asset_database.h"

#include "assets/asset.h"
#include "hash_file.h"

#include <cross/mapped_file.h>

#include <exo/collections/handle_map.h>
#include <exo/hash.h>
#include <exo/serialization/index_map_serializer.h>
#include <exo/serialization/pool_serializer.h>
#include <exo/serialization/serializer.h>
#include <exo/serialization/uuid_serializer.h>
#include <exo/uuid.h>

#include <filesystem>

AssetDatabase AssetDatabase::create()
{
	AssetDatabase database        = {};
	database.asset_id_map         = exo::IndexMap::with_capacity(32);
	database.resource_path_map    = exo::IndexMap::with_capacity(64);
	database.resource_content_map = exo::IndexMap::with_capacity(64);
	return database;
}

// -- Resources

void AssetDatabase::track_resource_changes(const exo::Path &directory, Vec<Handle<Resource>> &out_outdated_resources)
{
	// Try to track moved/outdated resources from disk
	auto fs_path = std::filesystem::path(directory.view());
	for (const auto &file_entry : std::filesystem::recursive_directory_iterator{fs_path}) {
		if (file_entry.is_regular_file() == false) {
			continue;
		}

		const auto &file_path     = exo::Path::from_string(file_entry.path().string());
		u64         path_hash     = hash_value(file_path);
		u64         resource_hash = 0;
		{
			auto resource_file = cross::MappedFile::open(file_path.view()).value();
			resource_hash      = assets::hash_file64(resource_file.content());
		}

		Option<u64> content_map_entry = this->resource_content_map.at(resource_hash);
		Option<u64> path_map_entry    = this->resource_path_map.at(path_hash);

		if (path_map_entry.has_value() && content_map_entry.has_value()) {
			// The resource is known
			auto resource_handle = exo::as_handle<Resource>(path_map_entry.value());
			if (!this->resource_records.get(resource_handle).asset_id.is_valid()) {
				out_outdated_resources.push_back(resource_handle);
			}
		} else if (path_map_entry.has_value() && !content_map_entry.has_value()) {
			// Only the content changed
			auto resource_handle = exo::as_handle<Resource>(path_map_entry.value());

			const u64 old_file_hash = this->resource_records.get(resource_handle).last_imported_hash;
			this->resource_content_map.remove(old_file_hash);
			this->resource_content_map.insert(resource_hash, path_map_entry.value());

			out_outdated_resources.push_back(resource_handle);
		} else if (!path_map_entry.has_value() && content_map_entry.has_value()) {
			// Only the path changed
			auto resource_handle = exo::as_handle<Resource>(content_map_entry.value());

			const auto &old_path      = this->resource_records.get(resource_handle).resource_path;
			const u64   old_path_hash = hash_value(old_path);
			this->resource_path_map.remove(old_path_hash);
			this->resource_path_map.insert(path_hash, content_map_entry.value());
			this->resource_records.get(resource_handle).resource_path = file_path;
		} else if (!path_map_entry.has_value() && !content_map_entry.has_value()) {
			// It's a new resource
			Resource new_record      = {};
			new_record.asset_id      = AssetId::invalid();
			new_record.resource_path = file_path;
			auto new_record_handle   = this->resource_records.add(std::move(new_record));
			this->resource_path_map.insert(path_hash, exo::to_u64(new_record_handle));
			this->resource_content_map.insert(resource_hash, exo::to_u64(new_record_handle));

			out_outdated_resources.push_back(new_record_handle);
		}
	}
}

Resource &AssetDatabase::get_resource_from_path(const exo::Path &path)
{
	u64  path_hash = hash_value(path);
	auto handle    = exo::as_handle<Resource>(this->resource_path_map.at(path_hash).value());
	return this->resource_records.get(handle);
}

Resource &AssetDatabase::get_resource_from_content(u64 content_hash)
{
	auto handle = exo::as_handle<Resource>(this->resource_content_map.at(content_hash).value());
	return this->resource_records.get(handle);
}

// -- Assets
Asset *AssetDatabase::get_asset(const AssetId &id)
{
	u64         id_hash = exo::hash_value(id);
	Option<u64> ptr     = this->asset_id_map.at(id_hash);
	if (ptr.has_value()) {
		auto *asset = reinterpret_cast<Asset *>(ptr.value());
		ASSERT(asset);
		return asset;
	}
	return nullptr;
}

void AssetDatabase::insert_asset(Asset *asset)
{
	ASSERT(asset);
	u64 id_hash = exo::hash_value(asset->uuid);
	u64 ptr     = reinterpret_cast<u64>(asset);
	this->asset_id_map.insert(id_hash, ptr);
}

// -- Serialization

void serialize(exo::Serializer &serializer, Resource &data)
{
	exo::serialize(serializer, data.asset_id);

	if (serializer.is_writing) {
		auto path_string = data.resource_path.view();
		auto path_c_str  = path_string.data();
		exo::serialize(serializer, path_c_str);
	} else {
		const char *path_string = "";
		exo::serialize(serializer, path_string);
		data.resource_path = exo::Path::from_string(path_string);
	}

	exo::serialize(serializer, data.last_imported_hash);
}

void serialize(exo::Serializer &serializer, AssetDatabase &db)
{
	exo::serialize(serializer, db.resource_path_map);
	exo::serialize(serializer, db.resource_content_map);
	exo::serialize(serializer, db.resource_records);
}
