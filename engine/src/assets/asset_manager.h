#pragma once
#include <exo/collections/map.h>
#include <exo/collections/vector.h>
#include <exo/collections/dynamic_array.h>
#include <exo/base/logger.h>

#include <exo/cross/uuid.h>
#include "assets/importers/generic_importer.h"
#include "assets/asset.h"

#include <concepts>
#include <string_view>
#include <filesystem>
#include <variant>


/**
   An asset is a single piece of content (a texture, a model, a material, etc) that has been imported from a resource (a file with an extension like jpg, ktx, gltf, etc).
   A single resource CAN contain multiple assets (a gltf can contains multiple models, textures and materials).

   Each resource is a file that has a metadata file associated that contains a UUID, a name, a hash and import settings.

   model.blend -> model.blend.meta (unique GUID)
   Import process generates 3 textures, 5 materials, 3 meshes assets and a subscene asset containing a hierarchy of entities
   Each "sub"-asset has a unique GUID wich is contained in the asset->internal_dependencies
   Every "sub"-asset (model + dependencies) will be compiled! but only the original model.blend is present in the Asset folder

   Each importer CAN import a specific type of resource. This importer can create assets of different types.
   Every importer MUST have one import settings struct and CAN use the same as other importers.
   The meta file will be different for difference resource's type and will contain the import settings.
   So there needs to be a way to idenitfy each import type and its associated importer and import settings
   Each importer can define an implicit resource type with the `can_import` function. Thus there is only a need to find the import settings corresponding to the importer.

 **/

namespace cross { struct FileWatcher; }
namespace UI {struct Context;}

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
    usize offset;
    const char* error_message;
};

// Meta files that identifies resources
struct ResourceMeta
{
    cross::UUID uuid;
    std::string display_name;
    std::filesystem::path resource_path;
    std::filesystem::path meta_path;
    void *importer_data; // importer specific data (import settings, internal UUIDs, etc)
    u64 last_imported_hash;
};

// Meta files that identifies assets
struct AssetMeta
{
    cross::UUID uuid;
    std::string display_name;
    u64 asset_hash;
};

using AssetLoaderFunc = Asset* (*)(cross::UUID asset_uuid, const void *file_data, usize file_size, void *user_data);
struct AssetLoader
{
    char file_identifier[5] = {};
    AssetLoaderFunc func = nullptr;
    void *user_data = nullptr;
};


struct AssetManager
{
    AssetManager() = default;
    AssetManager(const AssetManager &other) = delete;
    AssetManager &operator=(const AssetManager &other) = delete;
    AssetManager(AssetManager &&other) = default;
    AssetManager &operator=(AssetManager &&other) = default;

    static AssetManager create();
    void init();
    void destroy();
    void setup_file_watcher(cross::FileWatcher &watcher);

    void display_ui(UI::Context &ui);

    // -- Resource files

    // Used by importers to import resources that needs a different importer
    Result<Asset*> import_resource(const void *data, usize len, void *import_settings = nullptr, u32 i_importer = u32_invalid, cross::UUID resource_uuid = {});
    Result<Asset*> import_resource(cross::UUID resource_uuid);

    // import the resource if needed and load its associated asset and its dependencies
    void load_resource(cross::UUID resource_uuid);

     // -- Asset files
    Result<Asset*> get_asset(cross::UUID asset_uuid);

    inline const Map<cross::UUID, AssetMeta> &get_assets_metadata() const { return asset_metadatas; }
    inline const Map<cross::UUID, Asset*> &get_assets() const { return assets; }

    // Registers a new asset type identified by the file_identifier
    template <std::derived_from<Asset> AssetType>
    void add_asset_loader(const char (&file_identifier)[5]);

    // Used by importers to create an asset manually
    template<std::derived_from<Asset> AssetType>
    AssetType* create_asset(cross::UUID uuid = {});

    // Used by importers when the asset created manually has finished importing and needs to be saved to disk
    Result<void> save_asset(Asset *asset);

    // Load an imported asset and its dependencies from disk
    Result<Asset*> load_asset(cross::UUID asset_uuid);

    void unload_asset(cross::UUID asset_uuid);

    // Load or import a resource if it hasnt been imported yet
    // TODO: support asset_uuid and import corresponding resource?
    Result<Asset*> load_or_import_resource(cross::UUID resource_uuid);

    // Utility

    static auto get_error_handlers()
    {
        return std::make_tuple(
            [](leaf::match<AssetErrors, AssetErrors::NoImporterFound>, const leaf::e_file_name &file)
            {
                logger::error("[AssetManager] No importer found for {}\n", file.value);
            },
            [](leaf::match<AssetErrors, AssetErrors::NoImporterFound>)
            {
                logger::error("[AssetManager] No importer found for in-memory resource\n");
            },
            [](leaf::match<AssetErrors, AssetErrors::NoLoaderFound>, cross::UUID asset_uuid)
            {
                logger::error("[AssetManager] No loader found for asset {}\n", asset_uuid);
            },
            [](leaf::match<AssetErrors, AssetErrors::ParsingError>, const JsonError &json_error)
            {
                logger::error("[AssetManager] JSON Parsing error: {}\n", json_error.error_message);
            },
            [](leaf::match<AssetErrors, AssetErrors::InvalidUUID>, cross::UUID invalid_uuid)
            {
                logger::info("[AssetManager] Invalid UUID: {}\n", invalid_uuid);
            },
            [](const leaf::error_info &unmatched)
            {
                logger::error("[AssetManager] Unknown error {}\n", unmatched);
            }
        );
    }


private:

    Result<u32> find_importer(const void *data, usize len);

    // -- Metadata files

    // Check if a file has a meta file on disk associated with it
    bool has_meta_file(const std::filesystem::path &file_path);

    // Create a new meta for a resource
    Result<cross::UUID> create_resource_meta(const std::filesystem::path &file_path);

    // Save a meta from memory to disk
    Result<void> save_resource_meta(GenericImporter &importer, ResourceMeta &meta);

    // Load a meta from disk to memory
    Result<cross::UUID> load_resource_meta(GenericImporter &importer, const std::filesystem::path &file_path);

    // Create a new meta for a asset
    Result<AssetMeta&> create_asset_meta(cross::UUID uuid);

    // Save a meta from memory to disk
    Result<void> save_asset_meta(AssetMeta &meta);

    // Load a meta from disk to memory
    Result<AssetMeta&> load_asset_meta(cross::UUID uuid);



    // "Original" assets to import are imported from this directory along their meta files
    std::filesystem::path resources_directory = ASSET_PATH;

    // All assets in memory are loaded from this directory, each asset is guid.ext
    std::filesystem::path assets_directory = COMPILED_ASSET_PATH;

    // Assets in memory
    Map<cross::UUID, Asset*> assets;

    // All assets metadata
    Map<cross::UUID, ResourceMeta> resource_metadatas;
    Map<cross::UUID, AssetMeta> asset_metadatas;

    DynamicArray<GenericImporter, 16> importers; // import resource into assets
    DynamicArray<AssetLoader, 16> loaders; // load assets on memory
};


// -- Template Implementations

template <std::derived_from<Asset> AssetType>
void AssetManager::add_asset_loader(const char (&file_identifier)[5])
{
    loaders.push_back(AssetLoader{
        .file_identifier = {file_identifier[0], file_identifier[1], file_identifier[2], file_identifier[3], file_identifier[4]},
        .func = [](cross::UUID asset_uuid, const void *file_data, usize file_size, void *user_data) -> Asset *
        {
            auto *self    = reinterpret_cast<AssetManager *>(user_data);
            auto *derived = self->create_asset<AssetType>(asset_uuid);
            derived->from_flatbuffer(file_data, file_size);
            derived->state = AssetState::Loaded;
            return derived;
        },
        .user_data = this,
    });
}

template <std::derived_from<Asset> AssetType>
AssetType *AssetManager::create_asset(cross::UUID uuid)
{
    if (!uuid.is_valid())
    {
        uuid = cross::UUID::create();
    }
    ASSERT(assets.contains(uuid) == false);
    ASSERT(uuid.is_valid());

    AssetType *new_asset = new AssetType();
    assets[uuid]         = reinterpret_cast<Asset *>(new_asset);
    new_asset->uuid      = uuid;
    return new_asset;
}
