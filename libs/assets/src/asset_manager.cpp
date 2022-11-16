#include "assets/asset_manager.h"
#include "assets/asset.h"
#include "assets/asset_id_formatter.h"
#include "assets/importers/gltf_importer.h"
#include "assets/importers/ktx2_importer.h"
#include "assets/importers/png_importer.h"
#include "cross/jobmanager.h"
#include "cross/jobs/custom.h"
#include "cross/mapped_file.h"
#include "exo/collections/span.h"
#include "exo/format.h"
#include "exo/logger.h"
#include "exo/memory/scope_stack.h"
#include "exo/serialization/serializer.h"
#include "exo/serialization/serializer_helper.h"
#include "exo/uuid_formatter.h"
#include "hash_file.h"
#include "reflection/reflection.h"
#include "reflection/reflection_serializer.h"
#include <filesystem>

static const exo::Path AssetPath         = exo::Path::from_string(ASSET_PATH);
static const exo::Path DatabasePath      = exo::Path::from_string(DATABASE_PATH);
static const exo::Path CompiledAssetPath = exo::Path::from_string(COMPILED_ASSET_PATH);

exo::Path AssetManager::get_asset_path(const AssetId &id)
{
	const exo::String filename = id.name + exo::StringView{".asset"};
	return exo::Path::join(CompiledAssetPath, filename);
}

AssetManager AssetManager::create(cross::JobManager &jobmanager)
{
	AssetManager asset_manager = {};
	asset_manager.jobmanager   = &jobmanager;

	asset_manager.importers.push_back(new GLTFImporter{});
	asset_manager.importers.push_back(new PNGImporter{});
	asset_manager.importers.push_back(new KTX2Importer{});

	const auto database_path = std::filesystem::path{DatabasePath.view().data()};
	if (std::filesystem::exists(database_path)) {
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
		exo::logger::error("Importer not found. %s\n", path.view().data());
		return;
	}

	auto &importer = *manager.importers[i_found_importer];

	// Create a new asset for this resource
	CreateRequest create_req{};
	create_req.asset = std::move(id);
	create_req.path  = path;
	auto create_resp = std::move(importer.create_asset(create_req).value());
	ASSERT(create_resp.new_id.is_valid());

	// Create and process its dependencies
	for (u32 i_dep = 0; i_dep < create_resp.dependencies_id.len(); ++i_dep) {
		import_resource(manager, create_resp.dependencies_id[i_dep], create_resp.dependencies_paths[i_dep]);
	}

	// Process this new asset
	ImporterApi    api{manager};
	ProcessRequest process_req{.importer_api = api};
	process_req.asset = create_resp.new_id;
	process_req.path  = path;
	auto process_resp = std::move(importer.process_asset(process_req).value());
	ASSERT(!process_resp.products.is_empty());

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

void AssetManager::_import_resources(exo::Span<const Handle<Resource>> records)
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
	auto       asset_path = AssetManager::get_asset_path(id);
	const auto fs_path    = std::filesystem::path{asset_path.view().data()};
	ASSERT(std::filesystem::exists(fs_path));
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
	exo::logger::info("Saving %s\n", asset_path.view().data());
}

static exo::Path get_blob_path(exo::u128 blob_hash)
{
	exo::ScopeStack scope;

	u64 blob_hash0 = 0;
	u64 blob_hash1 = 0;
	exo::u128_to_u64(blob_hash, &blob_hash0, &blob_hash1);
	auto blob_filename = exo::formatf(scope, "%x%x.bin", blob_hash0, blob_hash1);

	return exo::Path::join(CompiledAssetPath, blob_filename);
}

void AssetManager::update_async()
{
	auto to_remove = exo::Vec<AssetId>::with_capacity(this->database.asset_async_waiting_for_deps.size);

	// Update the state of assets waiting for their dependencies to load.
	for (const auto &asset_id : this->database.asset_async_waiting_for_deps) {
		auto asset = this->database.get_asset(asset_id);
		ASSERT(asset->state == AssetState::LoadedWaitingForDeps);

		u32 loaded_deps = 0;
		for (const auto &dep : asset->dependencies) {
			if (this->is_fully_loaded(dep)) {
				loaded_deps += 1;
			}
		}
		if (loaded_deps == asset->dependencies.len()) {
			asset->state = AssetState::FullyLoaded;
			to_remove.push(asset_id);
		}
	}
	for (const auto &asset_id : to_remove) {
		this->database.asset_async_waiting_for_deps.remove(asset_id);
	}

	// Update the state of assets that finished loading asynchronously
	to_remove.clear();
	to_remove.reserve(this->database.asset_async_requests.size);
	for (const auto &[asset_id, req] : this->database.asset_async_requests) {
		if (req.waitable->is_done()) {
			this->finish_loading_async(req.data->result);
			to_remove.push(asset_id);
		}
	}
	for (const auto &asset_id : to_remove) {
		this->database.asset_async_requests.remove(asset_id);
	}
}

void AssetManager::load_asset_async(const AssetId &id)
{
	ASSERT(!this->is_loaded(id));

	// Avoid loading the same assets twice
	if (this->database.asset_async_requests.at(id)) {
		return;
	}

	printf("[AssetManager] Loading %s asynchronously.\n", id.name.c_str());

	auto *req           = this->database.asset_async_requests.insert(id, {});
	req->data           = std::make_unique<AssetAsyncRequest::Data>();
	req->data->asset_id = id;

	req->waitable = cross::custom_job<AssetAsyncRequest::Data>(*this->jobmanager,
		req->data.get(),
		[](AssetAsyncRequest::Data *data) { data->result = AssetManager::_load_from_disk(data->asset_id); });
}

void AssetManager::finish_loading_async(refl::BasePtr<Asset> asset)
{
	printf("[AssetManager] Finished loading [%s](%s) asynchronously.\n",
		asset.typeinfo().name,
		asset->uuid.name.c_str());

	this->database.insert_asset(asset);

	u32 deps_requests = 0;
	for (const auto &dep : asset->dependencies) {
		if (!this->is_loaded(dep)) {
			this->load_asset_async(dep);
			deps_requests += 1;
		}
	}

	if (deps_requests > 0) {
		asset->state = AssetState::LoadedWaitingForDeps;
		this->database.asset_async_waiting_for_deps.insert(asset->uuid);
	} else {
		asset->state = AssetState::FullyLoaded;
	}
}

void AssetManager::unload_asset(const AssetId &id) { this->database.remove_asset(id); }

usize AssetManager::read_blob(exo::u128 blob_hash, exo::Span<u8> out_data)
{
	auto path         = get_blob_path(blob_hash);
	auto blob_file    = cross::MappedFile::open(path.view()).value();
	auto blob_content = blob_file.content();
	ASSERT(out_data.len() >= blob_content.len());
	std::memcpy(out_data.data(), blob_content.data(), blob_content.len());
	return blob_content.len();
}

exo::u128 AssetManager::save_blob(exo::Span<const u8> blob_data)
{
	auto blob_hash = assets::hash_file128(blob_data);
	auto path      = get_blob_path(blob_hash);

	FILE *fp       = fopen(path.view().data(), "wb");
	auto  bwritten = fwrite(blob_data.data(), 1, blob_data.len(), fp);
	ASSERT(bwritten == blob_data.len());
	fclose(fp);

	return blob_hash;
}
