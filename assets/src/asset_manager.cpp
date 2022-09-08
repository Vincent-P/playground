#include "assets/asset_manager.h"

#include "assets/asset.h"
#include "assets/asset_constructors.h"
#include "assets/asset_id_formatter.h"
#include "assets/importers/gltf_importer.h"
#include "assets/importers/ktx2_importer.h"
#include "assets/importers/png_importer.h"

#include "hash_file.h"

#include <exo/collections/handle_map.h>
#include <exo/collections/index_map.h>
#include <exo/logger.h>
#include <exo/memory/scope_stack.h>
#include <exo/serializer.h>
#include <exo/serializer_helper.h>
#include <exo/uuid_formatter.h>

#include <cross/mapped_file.h>

#include <filesystem>
#include <fmt/core.h>
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

AssetManager *AssetManager::create(exo::ScopeStack &scope)
{
	auto *asset_manager = scope.allocate<AssetManager>();

	asset_manager->importers.push_back(new GLTFImporter{});
	asset_manager->importers.push_back(new PNGImporter{});
	asset_manager->importers.push_back(new KTX2Importer{});

	if (std::filesystem::exists(DatabasePath.view())) {
		auto resource_file = cross::MappedFile::open(DatabasePath.view()).value();
		exo::serializer_helper::read_object(resource_file.content(), asset_manager->database);
		asset_manager->database.asset_id_map = exo::IndexMap::with_capacity(32);
	} else {
		asset_manager->database = AssetDatabase::create();
	}

	Vec<Handle<Resource>> outdated_resources;
	asset_manager->database.track_resource_changes(AssetPath, outdated_resources);
	asset_manager->import_resources(outdated_resources);

	exo::serializer_helper::write_object_to_file(DatabasePath.view(), asset_manager->database);

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
	create_req.asset = id;
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
	u64  resource_hash = assets::hash_file(resource_file.content());
	resource_file.close();
	auto &asset_record = manager.database.get_resource_from_content(resource_hash);
	if (asset_record.asset_id != process_req.asset) {
		ASSERT(!asset_record.asset_id.is_valid());
		asset_record.asset_id = process_req.asset;
	}
	asset_record.last_imported_hash = resource_hash;

	// write the assets produced by this resource to disk
	for (auto product : process_resp.products) {
		Asset *asset      = manager.load_asset<Asset>(product);
		auto   asset_path = AssetManager::get_asset_path(product);
		exo::serializer_helper::write_object_to_file(asset_path.view(), *asset);
		exo::logger::info("Saving {}\n", asset_path.view());
	}
}

void AssetManager::import_resources(std::span<const Handle<Resource>> records)
{
	for (auto handle : records) {
		auto       &asset_record = this->database.resource_records.get(handle);
		const auto &asset_path   = asset_record.resource_path;

		auto resource_file = cross::MappedFile::open(asset_path.view()).value();
		u64  resource_hash = assets::hash_file(resource_file.content());
		resource_file.close();

		if (asset_record.last_imported_hash != resource_hash) {
			import_resource(*this, asset_record.asset_id, asset_path);
		}
	}
}

Asset *AssetManager::load_from_disk(AssetId id)
{
	auto asset_path = AssetManager::get_asset_path(id);
	ASSERT(std::filesystem::exists(asset_path.view()));
	Asset		  *new_asset     = global_asset_constructors().create(id.type_id);
	exo::ScopeStack scope         = exo::ScopeStack::with_allocator(&exo::tls_allocator);
	auto            resource_file = cross::MappedFile::open(asset_path.view()).value();
	exo::Serializer serializer    = exo::Serializer::create(&scope);
	serializer.buffer_size        = resource_file.size;
	serializer.buffer             = const_cast<void *>(resource_file.base_addr);
	serializer.is_writing         = false;
	new_asset->serialize(serializer);
	return new_asset;
}
