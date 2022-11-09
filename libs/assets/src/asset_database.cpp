#include "assets/asset_database.h"

#include "assets/asset.h"
#include "hash_file.h"

#include <cross/jobmanager.h>
#include <cross/jobs/foreach.h>
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

// -- Resources
enum struct TrackerAction
{
	None,
	UpdateContentMap,
	UpdatePathMap,
	NewResource,
};

struct ResourceTracker
{
	exo::Path             resource_path;
	exo::Handle<Resource> resource;
	FileHash              hash;
	TrackerAction         action               = TrackerAction::None;
	bool                  is_resource_outdated = false;
};

void AssetDatabase::track_resource_changes(
	cross::JobManager &jobmanager, const exo::Path &directory, Vec<Handle<Resource>> &out_outdated_resources)
{
	Vec<ResourceTracker> trackers;

	// Try to track moved/outdated resources from disk
	auto fs_path = std::filesystem::path(directory.view());
	for (const auto &file_entry : std::filesystem::recursive_directory_iterator{fs_path}) {
		if (file_entry.is_regular_file() == false) {
			continue;
		}

		trackers.push_back({});
		auto &tracker         = trackers.back();
		tracker.resource_path = exo::Path::from_string(file_entry.path().string());
	}

	auto w = cross::parallel_foreach_userdata<ResourceTracker, const AssetDatabase, true>(
		jobmanager,
		std::span(trackers),
		this,
		[](ResourceTracker &tracker, const AssetDatabase *self) {
			auto resource_file = cross::MappedFile::open(tracker.resource_path.view()).value();
			tracker.hash       = FileHash{assets::hash_file64(resource_file.content())};
			resource_file.close();

			const auto *content_map_entry = self->resource_content_map.at(tracker.hash);
			const auto *path_map_entry    = self->resource_path_map.at(tracker.resource_path);

			if (path_map_entry && content_map_entry) {
				// The resource is known
				tracker.resource = *path_map_entry;

				// If the resource has no asset id in the database it is outdated
				if (!self->resource_records.get(tracker.resource).asset_id.is_valid()) {
					tracker.is_resource_outdated = true;
				}
			} else if (path_map_entry && !content_map_entry) {
				// Only the content changed
				tracker.resource             = *path_map_entry;
				tracker.is_resource_outdated = true;
				tracker.action               = TrackerAction::UpdateContentMap;
			} else if (!path_map_entry && content_map_entry) {
				// Only the path changed
				tracker.action   = TrackerAction::UpdatePathMap;
				tracker.resource = *content_map_entry;
			} else if (!path_map_entry && !content_map_entry) {
				// New resource
				tracker.action = TrackerAction::NewResource;
			}
		},
		8);
	w->wait();

	for (auto &tracker : trackers) {
		switch (tracker.action) {
		default:
		case TrackerAction::None: {
			break;
		}
		case TrackerAction::UpdateContentMap: {
			const auto old_file_hash = this->resource_records.get(tracker.resource).last_imported_hash;
			if (old_file_hash.hash != 0) {
				this->resource_content_map.remove(old_file_hash);
			}
			this->resource_content_map.insert(tracker.hash, tracker.resource);
			break;
		}
		case TrackerAction::UpdatePathMap: {
			const auto &old_path = this->resource_records.get(tracker.resource).resource_path;
			this->resource_path_map.remove(old_path);
			this->resource_path_map.insert(tracker.resource_path, tracker.resource);
			this->resource_records.get(tracker.resource).resource_path = tracker.resource_path;
			break;
		}
		case TrackerAction::NewResource: {
			Resource new_record      = {};
			new_record.asset_id      = AssetId::invalid();
			new_record.resource_path = tracker.resource_path;
			auto new_record_handle   = this->resource_records.add(std::move(new_record));
			this->resource_path_map.insert(tracker.resource_path, new_record_handle);
			this->resource_content_map.insert(tracker.hash, new_record_handle);
			break;
		}
		}

		if (tracker.is_resource_outdated) {
			out_outdated_resources.push_back(tracker.resource);
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
	ASSERT(id.is_valid());
	refl::BasePtr<Asset> *ptr = this->asset_id_map.at(id);
	if (ptr) {
		ASSERT(ptr->get());
		return *ptr;
	}
	return refl::BasePtr<Asset>::invalid();
}

void AssetDatabase::remove_asset(const AssetId &id) { this->asset_id_map.remove(id); }

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
