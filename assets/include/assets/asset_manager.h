#pragma once
#include <exo/collections/dynamic_array.h>
#include <exo/collections/map.h>
#include <exo/collections/vector.h>
#include <exo/logger.h>
#include <exo/memory/linear_allocator.h>
#include <exo/serializer.h>
#include <exo/uuid.h>

#include "assets/asset.h"
#include "assets/importers/generic_importer.h"

#include <filesystem>

/**
   An asset is a single piece of content (a texture, a model, a material, etc) that has been imported from a resource (a
 file with an extension like jpg, ktx, gltf, etc). A single resource CAN contain multiple assets (a gltf can contains
 multiple models, textures and materials).

   Each resource is a file that has a metadata file associated that contains a UUID, a name, a hash and import settings.

   model.blend -> model.blend.meta (unique GUID)
   Import process generates 3 textures, 5 materials, 3 meshes assets and a subscene asset containing a hierarchy of
 entities Each "sub"-asset has a unique GUID wich is contained in the asset->internal_dependencies Every "sub"-asset
 (model + dependencies) will be compiled! but only the original model.blend is present in the Asset folder

   Each importer CAN import a specific type of resource. This importer can create assets of different types.
   Every importer MUST have one import settings struct and CAN use the same as other importers.
   The meta file will be different for difference resource's type and will contain the import settings.
   So there needs to be a way to idenitfy each import type and its associated importer and import settings
   Each importer can define an implicit resource type with the `can_import` function. Thus there is only a need to find
 the import settings corresponding to the importer.
 **/

namespace cross
{
struct FileWatcher;
}
namespace exo
{
struct ScopeStack;
}

enum struct AssetErrors : int
{
	Invalid,
	NoImporterFound,
	NoLoaderFound,
	ParsingError,
	InvalidUUID,
};

struct JsonError
{
	usize       offset;
	const char *error_message;
};

// Meta files that identifies resources
struct ResourceMeta
{
	exo::UUID             uuid;
	const char           *display_name;
	std::filesystem::path resource_path;
	std::filesystem::path meta_path;
	void                 *importer_data; // importer specific data (import settings, internal UUIDs, etc)
	u64                   last_imported_hash;
};

// Meta files that identifies assets
struct AssetMeta
{
	exo::UUID   uuid;
	const char *display_name;
	u64         asset_hash;
};

struct AssetManager
{
	static AssetManager *create(exo::ScopeStack &scope);
	~AssetManager();

	void load_all_metas();
	void setup_file_watcher(cross::FileWatcher &watcher);

	// -- Resource files

	// Used by importers to import resources that needs a different importer
	Result<Asset *> import_resource(const void *data,
		usize                                   len,
		void                                   *import_settings = nullptr,
		u32                                     i_importer      = u32_invalid,
		exo::UUID                               resource_uuid   = {});
	Result<Asset *> import_resource(exo::UUID resource_uuid);

	// import the resource if needed and load its associated asset and its dependencies
	void load_resource(exo::UUID resource_uuid);

	// -- Asset files
	Result<Asset *> get_asset(exo::UUID asset_uuid);

	inline const exo::Map<exo::UUID, AssetMeta> &get_assets_metadata() const { return asset_metadatas; }
	inline const exo::Map<exo::UUID, Asset *>   &get_assets() const { return assets; }

	// Used by importers to create an asset manually
	template <std::derived_from<Asset> AssetType> AssetType *create_asset(exo::UUID uuid = {});

	void create_asset_internal(Asset *asset, exo::UUID uuid = {});

	// Used by importers when the asset created manually has finished importing and needs to be saved to disk
	Result<void> save_asset(Asset *asset);

	// Load an imported asset and its dependencies from disk
	Result<Asset *> load_asset(exo::UUID asset_uuid);

	void unload_asset(exo::UUID asset_uuid);

	// Load or import a resource if it hasnt been imported yet
	// TODO: support asset_uuid and import corresponding resource?
	Result<Asset *> load_or_import_resource(exo::UUID resource_uuid);

private:
	Result<u32> find_importer(const void *data, usize len);

	// -- Metadata files

	// Check if a file has a meta file on disk associated with it
	bool has_meta_file(const std::filesystem::path &file_path);

	// Create a new meta for a resource
	Result<exo::UUID> create_resource_meta(const std::filesystem::path &file_path);

	// Save a meta from memory to disk
	Result<void> save_resource_meta(GenericImporter &importer, ResourceMeta &meta);

	// Load a meta from disk to memory
	Result<exo::UUID> load_resource_meta(GenericImporter &importer, const std::filesystem::path &file_path);

	// Create a new meta for a asset
	Result<AssetMeta *> create_asset_meta(exo::UUID uuid);

	// Save a meta from memory to disk
	Result<void> save_asset_meta(AssetMeta &meta);

	// Load a meta from disk to memory
	Result<AssetMeta *> load_asset_meta(exo::UUID uuid);

	// "Original" assets to import are imported from this directory along their meta files
	std::filesystem::path resources_directory = ASSET_PATH;

	// All assets in memory are loaded from this directory, each asset is guid.ext
	std::filesystem::path assets_directory = COMPILED_ASSET_PATH;

	// Assets in memory
	exo::Map<exo::UUID, Asset *> assets;

	// All assets metadata
	exo::Map<exo::UUID, ResourceMeta> resource_metadatas;
	exo::Map<exo::UUID, AssetMeta>    asset_metadatas;

	exo::DynamicArray<GenericImporter, 16> importers; // import resource into assets
};

// -- Template Implementations

template <std::derived_from<Asset> AssetType> AssetType *AssetManager::create_asset(exo::UUID uuid)
{
	AssetType *new_asset = new AssetType();
	this->create_asset_internal(new_asset, uuid);
	return new_asset;
}
