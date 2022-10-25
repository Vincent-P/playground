#include "assets/asset_database.h"

#include "assets/asset.h"
#include "hash_file.h"

#include <cross/mapped_file.h>

#include <exo/hash.h>
#include <exo/serialization/handle_serializer.h>
#include <exo/serialization/map_serializer.h>
#include <exo/serialization/path_serializer.h>
#include <exo/serialization/pool_serializer.h>
#include <exo/serialization/serializer.h>
#include <exo/serialization/uuid_serializer.h>
#include <exo/uuid.h>

#include <filesystem>

AssetDatabase AssetDatabase::create()
{
	AssetDatabase database = {};
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

		const auto &file_path = exo::Path::from_string(file_entry.path().string());

		auto resource_file = cross::MappedFile::open(file_path.view()).value();
		auto resource_hash = FileHash{assets::hash_file64(resource_file.content())};
		resource_file.close();

		auto *content_map_entry = this->resource_content_map.at(resource_hash);
		auto *path_map_entry    = this->resource_path_map.at(file_path);

		if (path_map_entry && content_map_entry) {
			// The resource is known
			auto resource_handle = *path_map_entry;
			if (!this->resource_records.get(resource_handle).asset_id.is_valid()) {
				out_outdated_resources.push_back(resource_handle);
			}
		} else if (path_map_entry && !content_map_entry) {
			// Only the content changed
			auto resource_handle = *path_map_entry;

			const auto old_file_hash = this->resource_records.get(resource_handle).last_imported_hash;
			this->resource_content_map.remove(old_file_hash);
			this->resource_content_map.insert(resource_hash, resource_handle);

			out_outdated_resources.push_back(resource_handle);
		} else if (!path_map_entry && content_map_entry) {
			// Only the path changed
			auto resource_handle = *content_map_entry;

			const auto &old_path = this->resource_records.get(resource_handle).resource_path;
			this->resource_path_map.remove(old_path);
			this->resource_path_map.insert(file_path, *content_map_entry);
			this->resource_records.get(resource_handle).resource_path = file_path;
		} else if (!path_map_entry && !content_map_entry) {
			// It's a new resource
			Resource new_record      = {};
			new_record.asset_id      = AssetId::invalid();
			new_record.resource_path = file_path;
			auto new_record_handle   = this->resource_records.add(std::move(new_record));
			this->resource_path_map.insert(file_path, new_record_handle);
			this->resource_content_map.insert(resource_hash, new_record_handle);

			out_outdated_resources.push_back(new_record_handle);
		}
	}
}

Resource &AssetDatabase::get_resource_from_path(const exo::Path &path)
{
	auto handle = *this->resource_path_map.at(path);
	return this->resource_records.get(handle);
}

Resource &AssetDatabase::get_resource_from_content(FileHash content_hash)
{
	auto handle = *this->resource_content_map.at(content_hash);
	return this->resource_records.get(handle);
}

// -- Assets
refl::BasePtr<Asset> AssetDatabase::get_asset(const AssetId &id)
{
	refl::BasePtr<Asset> *ptr = this->asset_id_map.at(id);
	if (ptr) {
		ASSERT(ptr->get());
		return *ptr;
	}
	return refl::BasePtr<Asset>::invalid();
}

void AssetDatabase::insert_asset(refl::BasePtr<Asset> asset)
{
	ASSERT(asset.get());
	this->asset_id_map.insert(asset->uuid, asset);
}

// -- Serialization

void serialize(exo::Serializer &serializer, Resource &data)
{
	exo::serialize(serializer, data.asset_id);
	exo::serialize(serializer, data.resource_path);
	exo::serialize(serializer, data.last_imported_hash.hash);
}

void serialize(exo::Serializer &serializer, AssetDatabase &db)
{
	exo::serialize(serializer, db.resource_path_map);
	exo::serialize(serializer, db.resource_content_map);
	exo::serialize(serializer, db.resource_records);
}

void serialize(exo::Serializer &serializer, FileHash &hash) { exo::serialize(serializer, hash.hash); }
