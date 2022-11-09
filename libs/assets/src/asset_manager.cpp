#include "assets/asset_manager.h"

#include "assets/asset.h"
#include "assets/asset_id_formatter.h"
#include "assets/importers/gltf_importer.h"
#include "assets/importers/ktx2_importer.h"
#include "assets/importers/png_importer.h"
#include <cross/jobmanager.h>
#include <cross/jobs/custom.h>
#include <cross/mapped_file.h>
#include <exo/logger.h>
#include <exo/serialization/serializer.h>
#include <exo/serialization/serializer_helper.h>
#include <exo/uuid_formatter.h>
#include <reflection/reflection_serializer.h>

#include "hash_file.h"

#include <filesystem>
#include <fmt/core.h>
#include <reflection/reflection.h>
#include <span>

static const exo::Path AssetPath         = exo::Path::from_string(ASSET_PATH);
static const exo::Path DatabasePath      = exo::Path::from_string(DATABASE_PATH);
static const exo::Path CompiledAssetPath = exo::Path::from_string(COMPILED_ASSET_PATH);

exo::Path AssetManager::get_asset_path(AssetId id)
{
	std::string filename = std::move(id.name);
	filename += std::string_view{".asset"};
	return exo::Path::join(CompiledAssetPath, filename);
}

AssetManager AssetManager::create(cross::JobManager &jobmanager)
{
	AssetManager asset_manager = {};
	asset_manager.jobmanager   = &jobmanager;

	asset_manager.importers.push_back(new GLTFImporter{});
	asset_manager.importers.push_back(new PNGImporter{});
	asset_manager.importers.push_back(new KTX2Importer{});

	if (std::filesystem::exists(DatabasePath.view())) {
		auto resource_file = cross::MappedFile::open(DatabasePath.view()).value();
		exo::serializer_helper::read_object(resource_file.content(), asset_manager.database);
	}

	Vec<Handle<Resource>> outdated_resources;
	asset_manager.database.track_resource_changes(jobmanager, AssetPath, outdated_resources);
	asset_manager._import_resources(outdated_resources);

	exo::serializer_helper::write_object_to_file(DatabasePath.view(), asset_manager.database);

	return asset_manager;
}

static void import_resource(AssetManager &manager, AssetId id, const exo::Path &path)
{
	auto file_extension = path.extension();

	// Find an importer for this resource
	u32 i_found_importer = u32_invalid;
	for (u32 i_importer = 0; i_importer < manager.importers.size(); ++i_importer) {
		if (manager.importers[i_importer]->can_import_extension({&file_extension, 1})) {
			i_found_importer = i_importer;
			break;
		}
	}
	if (i_found_importer == u32_invalid) {
		exo::logger::error("Importer not found. {}\n", path.view());
		return;
	}

	auto &importer = *manager.importers[i_found_importer];

	// Create a new asset for this resource
	CreateRequest create_req{};
	create_req.asset = std::move(id);
	create_req.path  = path;
	auto create_resp = importer.create_asset(create_req).value();
	ASSERT(create_resp.new_id.is_valid());

	// Create and process its dependencies
	for (u32 i_dep = 0; i_dep < create_resp.dependencies_id.size(); ++i_dep) {
		import_resource(manager, create_resp.dependencies_id[i_dep], create_resp.dependencies_paths[i_dep]);
	}

	// Process this new asset
	ImporterApi    api{manager};
	ProcessRequest process_req{.importer_api = api};
	process_req.asset = create_resp.new_id;
	process_req.path  = path;
	auto process_resp = importer.process_asset(process_req).value();
	ASSERT(!process_resp.products.empty());

	// Update the resource in the database
	auto resource_file = cross::MappedFile::open(path.view()).value();
	auto resource_hash = FileHash{assets::hash_file64(resource_file.content())};
	resource_file.close();
	auto &asset_record = manager.database.get_resource_from_content(resource_hash);
	if (asset_record.asset_id != process_req.asset) {
		ASSERT(!asset_record.asset_id.is_valid());
		asset_record.asset_id = process_req.asset;
	}
	asset_record.last_imported_hash = resource_hash;

	// write the assets produced by this resource to disk
	for (const auto &product : process_resp.products) {
		auto asset = manager.load_asset(product);
		manager._save_to_disk(asset);
	}
}

void AssetManager::_import_resources(std::span<const Handle<Resource>> records)
{
	for (auto handle : records) {
		auto       &asset_record = this->database.resource_records.get(handle);
		const auto &asset_path   = asset_record.resource_path;

		auto resource_file = cross::MappedFile::open(asset_path.view()).value();
		auto resource_hash = FileHash{assets::hash_file64(resource_file.content())};
		resource_file.close();

		if (asset_record.last_imported_hash != resource_hash) {
			import_resource(*this, asset_record.asset_id, asset_path);
		}
	}
}

refl::BasePtr<Asset> AssetManager::_load_from_disk(const AssetId &id)
{
	auto asset_path = AssetManager::get_asset_path(id);
	ASSERT(std::filesystem::exists(asset_path.view()));
	auto resource_file = cross::MappedFile::open(asset_path.view()).value();
	auto new_asset     = refl::BasePtr<Asset>::invalid();
	exo::serializer_helper::read_object(resource_file.content(), new_asset);
	new_asset->state = AssetState::LoadedWaitingForDeps;
	return new_asset;
}

void AssetManager::_save_to_disk(refl::BasePtr<Asset> asset)
{
	auto asset_path = AssetManager::get_asset_path(asset->uuid);
	exo::serializer_helper::write_object_to_file(asset_path.view(), asset);
	exo::logger::info("Saving {}\n", asset_path.view());
}

static exo::Path get_blob_path(exo::u128 blob_hash)
{
	char buffer[64] = {};

	u64 blob_hash0 = 0;
	u64 blob_hash1 = 0;
	exo::u128_to_u64(blob_hash, &blob_hash0, &blob_hash1);
	auto res = fmt::format_to_n(buffer, 64, "{:x}{:x}.bin", blob_hash0, blob_hash1);

	auto blob_filename = std::string_view{buffer, res.size};
	return exo::Path::join(CompiledAssetPath, blob_filename);
}

void AssetManager::update_async()
{
	exo::Vec<AssetId> to_remove;
	to_remove.reserve(this->database.asset_async_requests.size);

	for (const auto &[asset_id, req] : this->database.asset_async_requests) {
		if (req.waitable->is_done()) {
			this->finish_loading_async(req.data->result);
			to_remove.push_back(asset_id);
		}
	}

	for (const auto &handle : to_remove) {
		this->database.asset_async_requests.remove(handle);
	}
}

void AssetManager::load_asset_async(const AssetId &id)
{
	ASSERT(!this->is_loaded(id));

	// Avoid loading the same assets twice
	if (this->database.asset_async_requests.at(id)) {
		return;
	}

	fmt::print("[AssetManager] Loading {} asynchronously.\n", id.name);

	auto *req           = this->database.asset_async_requests.insert(id, {});
	req->data           = std::make_unique<AssetAsyncRequest::Data>();
	req->data->asset_id = id;

	req->waitable = cross::custom_job<AssetAsyncRequest::Data>(*this->jobmanager,
		req->data.get(),
		[](AssetAsyncRequest::Data *data) { data->result = AssetManager::_load_from_disk(data->asset_id); });
}

void AssetManager::finish_loading_async(refl::BasePtr<Asset> asset)
{
	fmt::print("[AssetManager] Finished loading [{}]({}) asynchronously.\n", asset.typeinfo().name, asset->uuid.name);

	this->database.insert_asset(asset);

	u32 deps_requests = 0;
	for (const auto &dep : asset->dependencies) {
		if (!this->is_loaded(dep)) {
			this->load_asset_async(dep);
			deps_requests += 1;
		}
	}

	asset->state = deps_requests > 0 ? AssetState::LoadedWaitingForDeps : AssetState::FullyLoaded;
}

void AssetManager::unload_asset(AssetId id) { this->database.remove_asset(id); }

usize AssetManager::read_blob(exo::u128 blob_hash, std::span<u8> out_data)
{
	auto path         = get_blob_path(blob_hash);
	auto blob_file    = cross::MappedFile::open(path.view()).value();
	auto blob_content = blob_file.content();
	ASSERT(out_data.size() >= blob_content.size());
	std::memcpy(out_data.data(), blob_content.data(), blob_content.size());
	return blob_content.size();
}

exo::u128 AssetManager::save_blob(std::span<const u8> blob_data)
{
	auto blob_hash = assets::hash_file128(blob_data);
	auto path      = get_blob_path(blob_hash);

	FILE *fp       = fopen(path.view().data(), "wb");
	auto  bwritten = fwrite(blob_data.data(), 1, blob_data.size(), fp);
	ASSERT(bwritten == blob_data.size());
	fclose(fp);

	return blob_hash;
}
