#include "assets/asset_manager.h"

#include "ui.h"

#include "assets/asset.h"
#include "assets/asset_constructors.h"
#include "assets/importers/gltf_importer.h"
#include "assets/importers/png_importer.h"
#include "assets/importers/ktx2_importer.h"

#include <exo/memory/scope_stack.h>
#include <exo/os/mapped_file.h>
#include <exo/os/file_watcher.h>
#include <exo/os/uuid.h>
#include <exo/logger.h>

#include <exo/memory/string_repository.h>
#include <filesystem>
#include <cstdio>

#include <leaf.hpp>
#include <meow_hash_x64_aesni.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/filewritestream.h>
#include <string.h>
#include <imgui.h>

// -- Utils
namespace
{
static std::filesystem::path resource_path_to_meta_path(std::filesystem::path file_path)
{
    auto meta_path = std::move(file_path);
    meta_path += ".meta";
    return meta_path;
}

static u64 hash_file(const void *data, usize len)
{
    void *non_const_data = const_cast<void *>(data);
    auto meow_hash = MeowHash(MeowDefaultSeed, len, non_const_data);
    return static_cast<u64>(_mm_extract_epi64(meow_hash, 0));
}
}

// -- Asset Manager

AssetManager *AssetManager::create(exo::ScopeStack &scope)
{
    auto *asset_manager = scope.allocate<AssetManager>();

    asset_manager->importers.push_back(GLTFImporter{});
    asset_manager->importers.push_back(PNGImporter{});
    asset_manager->importers.push_back(KTX2Importer{});

    return asset_manager;
}

AssetManager::~AssetManager()
{
}

void AssetManager::load_all_metas()
{
    namespace leaf = exo::leaf;
    // Load or create all resources meta
    for (const auto &file_entry : std::filesystem::recursive_directory_iterator{this->resources_directory})
    {
        if (file_entry.is_regular_file() == false)
        {
            continue;
        }

        leaf::try_handle_all(
            [&]() -> Result<void>
            {
                const auto &file_path = file_entry.path();

                Result<os::UUID> new_uuid;
                if (this->has_meta_file(file_path))
                {
                    auto resource_file = os::MappedFile::open(file_path.string()).value();
                    BOOST_LEAF_AUTO(i_importer, this->find_importer(resource_file.base_addr, resource_file.size));
                    new_uuid = this->load_resource_meta(importers[i_importer], file_path);
                }
                else
                {
                    new_uuid = this->create_resource_meta(file_path);
                }

                BOOST_LEAF_CHECK(new_uuid);

                return Ok<void>();
            },
            this->get_error_handlers());
    }

    exo::logger::info("[AssetManager] Done checking in all resources.\n");

    // process all assets
    for (const auto &file_entry : std::filesystem::recursive_directory_iterator{this->assets_directory})
    {
        if (file_entry.is_regular_file() == false)
        {
            continue;
        }
        auto filename = file_entry.path().filename().string();
        if (filename.size() != os::UUID::STR_LEN) {
            continue;
        }

        leaf::try_handle_all(
            [&]() -> Result<void>
            {
                auto uuid     = os::UUID::from_string(filename.c_str(), filename.size());
                exo::logger::info("[AssetManager] Found asset {}.\n", filename);

                if (this->has_meta_file(file_entry.path()))
                {
                    BOOST_LEAF_CHECK(this->load_asset_meta(uuid));
                }
                else
                {
                    BOOST_LEAF_CHECK(this->create_asset_meta(uuid));
                }
                return Ok<void>();
            },
            this->get_error_handlers());
    }
}

void AssetManager::setup_file_watcher(os::FileWatcher &watcher)
{
    int assets_wd = watcher.add_watch(ASSET_PATH).wd;

    // TODO: Properly watch file system changes and respond accordingly
    watcher.on_file_change(
        [&, assets_wd](const os::Watch &watch, const os::WatchEvent &event)
        {
            if (watch.wd != assets_wd)
            {
                return;
            }

            const char *p = "";
            switch (event.action)
            {
            case os::WatchEventAction::FileChanged:
                p = "file changed: ";
                break;
            case os::WatchEventAction::FileRemoved:
                p = "file removed: ";
                break;
            case os::WatchEventAction::FileAdded:
                p = "file added: ";
                break;
            case os::WatchEventAction::FileRenamed:
                p = "file renamed: ";
                break;
            }

            auto file_path = this->resources_directory / event.name;

            exo::logger::info("[AssetManager] {} {}\n", p, file_path);

            if (event.action == os::WatchEventAction::FileChanged || event.action == os::WatchEventAction::FileAdded)
            {
                if (!this->has_meta_file(file_path))
                {
                    this->create_resource_meta(file_path);
                }
            }

            if (event.action == os::WatchEventAction::FileRemoved)
            {
                // unload asset, remove meta from memory and filesystem
            }

            if (event.action == os::WatchEventAction::FileRenamed)
            {
            }
        });
}

void AssetManager::display_ui()
{
    namespace leaf = exo::leaf;
    ZoneScoped;

    static os::UUID s_selected = {};

    if (auto w = UI::begin_window("AssetManager"))
    {
        auto table_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInner;


        ImGui::Text("Loaded assets");
        os::UUID to_remove = {};
        if (ImGui::BeginTable("AssetsTable", 6, table_flags))
        {
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("UUID");
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("State");
            ImGui::TableSetupColumn("Asset Hash");
            ImGui::TableSetupColumn("Actions");
            ImGui::TableHeadersRow();

            for (const auto &[uuid, asset] : assets)
            {
                ImGui::TableNextRow();
                ImGui::PushID(asset);

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", asset == nullptr ? "null" : asset->type_name());

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.*s", static_cast<int>(os::UUID::STR_LEN), uuid.str);

                if (asset_metadatas.contains(uuid))
                {
                    const auto &metadata = asset_metadatas[uuid];

                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%s", metadata.display_name);

                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%zX", metadata.asset_hash);
                }

                ImGui::TableSetColumnIndex(3);
                const char *asset_state = asset == nullptr ? "null" : to_string(asset->state);
                ImGui::Text("%s", asset_state);

                ImGui::TableSetColumnIndex(5);
                if (ImGui::Button("Unload"))
                {
                    to_remove = uuid;
                }

                if (ImGui::Button("Select"))
                {
                    s_selected = uuid;
                }
                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        if (to_remove.is_valid())
        {
            unload_asset(to_remove);
        }

        ImGui::Separator();
        ImGui::Text("Resources metadata");
        if (ImGui::BeginTable("ResourcesMetadataTable", 5, table_flags))
        {
            ImGui::TableSetupColumn("UUID");
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Resource path");
            ImGui::TableSetupColumn("Last imported hash");
            ImGui::TableSetupColumn("Actions");
            ImGui::TableHeadersRow();

            for (const auto &[uuid, resource_meta] : resource_metadatas)
            {
                ImGui::TableNextRow();
                ImGui::PushID(&resource_meta);

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%.*s", static_cast<int>(os::UUID::STR_LEN), uuid.str);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", resource_meta.display_name);

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%s", resource_meta.resource_path.string().c_str());

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%zX", resource_meta.last_imported_hash);

                ImGui::TableSetColumnIndex(4);
                if (ImGui::Button("Load"))
                {
                    auto uuid_copy = uuid;
                    leaf::try_handle_all(
                        [&]() -> Result<void> { BOOST_LEAF_CHECK(load_or_import_resource(uuid_copy)); return Ok<void>(); },
                        get_error_handlers());
                }
                if (ImGui::Button("Import"))
                {
                    auto uuid_copy = uuid;
                    leaf::try_handle_all(
                        [&]() -> Result<void> { BOOST_LEAF_CHECK(import_resource(uuid_copy)); return Ok<void>(); },
                        get_error_handlers());
                }
                ImGui::PopID();
            }

            ImGui::EndTable();
        }
        ImGui::Separator();
        ImGui::Text("Assets metadata");
        if (ImGui::BeginTable("AssetMetadataTable", 4, table_flags))
        {
            ImGui::TableSetupColumn("UUID");
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Hash");
            ImGui::TableSetupColumn("Actions");
            ImGui::TableHeadersRow();

            for (const auto &[uuid, asset_meta] : asset_metadatas)
            {
                auto uuid_copy = uuid;
                ImGui::TableNextRow();

                ImGui::PushID(&asset_meta);

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%.*s", static_cast<int>(os::UUID::STR_LEN), uuid.str);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", asset_meta.display_name);

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%zX", asset_meta.asset_hash);

                ImGui::TableSetColumnIndex(3);
                if (ImGui::Button("Load"))
                {
                    leaf::try_handle_all(
                        [&]() -> Result<void> { BOOST_LEAF_CHECK(load_asset_meta(uuid_copy)); return Ok<void>(); },
                        get_error_handlers());
                }
                ImGui::SameLine();
                if (ImGui::Button("Save"))
                {
                    leaf::try_handle_all(
                        [&]() -> Result<void> { BOOST_LEAF_CHECK(save_asset_meta(asset_metadatas[uuid_copy])); return Ok<void>(); },
                        get_error_handlers());
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }

    if (auto w = UI::begin_window("Asset"))
    {
        if (s_selected.is_valid() && assets.contains(s_selected))
        {
            ImGui::Separator();
            Asset *selected_asset = assets.at(s_selected);
            ImGui::Text("Selected %s", selected_asset->name);
            selected_asset->display_ui();
            ImGui::Separator();
        }
    }
}


// -- Resource files

Result<Asset *> AssetManager::import_resource(const void *data, usize len, void *importer_data, u32 i_importer, os::UUID resource_uuid)
{
    if (i_importer == u32_invalid)
    {
        BOOST_LEAF_ASSIGN(i_importer, find_importer(data, len));
    }
    return importers[i_importer].import(this, resource_uuid, data, len, importer_data);
}

Result<Asset *> AssetManager::import_resource(os::UUID resource_uuid)
{
    // Invalid UUID?
    ASSERT(resource_metadatas.contains(resource_uuid));
    exo::logger::info("[AssetManager] importing resource {} from disk\n", resource_uuid);

    auto &resource_meta = resource_metadatas.at(resource_uuid);
    auto load = exo::leaf::on_error(exo::leaf::e_file_name{resource_meta.resource_path.string()});

    auto resource_file = os::MappedFile::open(resource_meta.resource_path.string()).value();

    u64  file_hash   = hash_file(resource_file.base_addr, resource_file.size);

    // TODO: only check if debug checks are enabled
    // ASSERT(resource_meta.last_imported_hash != file_hash);

    BOOST_LEAF_AUTO(i_importer, find_importer(resource_file.base_addr, resource_file.size));
    BOOST_LEAF_AUTO(new_asset, this->import_resource(resource_file.base_addr, resource_file.size, resource_meta.importer_data, i_importer, resource_uuid));

    resource_meta.last_imported_hash = file_hash;
    BOOST_LEAF_CHECK(save_resource_meta(importers[i_importer], resource_meta));

    return Ok(new_asset);
}

// -- Asset files
Result<Asset*> AssetManager::get_asset(os::UUID asset_uuid)
{
    if (assets.contains(asset_uuid))
    {
        return Ok(assets.at(asset_uuid));
    }
    return Err(AssetErrors::InvalidUUID, asset_uuid);
}


void AssetManager::create_asset_internal(Asset *asset, os::UUID uuid)
{
    if (!uuid.is_valid())
    {
        uuid = os::UUID::create();
    }
    ASSERT(assets.contains(uuid) == false);
    ASSERT(uuid.is_valid());

    assets[uuid]         = asset;
    asset->uuid      = uuid;
}

Result<void> AssetManager::save_asset(Asset *asset)
{
    exo::ScopeStack scope = exo::ScopeStack::with_allocator(&exo::tls_allocator);
    exo::Serializer serializer  = exo::Serializer::create(&scope);
    serializer.buffer_size = 10_MiB;
    serializer.buffer      = scope.allocate(serializer.buffer_size);
    serializer.is_writing = true;
    asset->serialize(serializer);

    // Save asset to disk
    auto  asset_path = assets_directory / asset->uuid.as_string();
    FILE *fp         = fopen(asset_path.string().c_str(), "wb"); // non-Windows use "w"
    auto  bwritten   = fwrite(serializer.buffer, 1, serializer.offset, fp);
    ASSERT(bwritten == serializer.offset);

    fclose(fp);

    // Load or create asset meta
    if (has_meta_file(asset_path))
    {
        BOOST_LEAF_CHECK(load_asset_meta(asset->uuid));
    }
    else
    {
        BOOST_LEAF_CHECK(create_asset_meta(asset->uuid));
    }

    return Ok<void>();
}

Result<Asset*> AssetManager::load_asset(os::UUID asset_uuid)
{
    if (assets.contains(asset_uuid))
    {
        exo::logger::info("[AssetManager] loading asset {} from memory\n", asset_uuid);
        return assets.at(asset_uuid);
    }
    exo::logger::info("[AssetManager] loading asset {} from disk\n", asset_uuid);

    auto asset_path = assets_directory / asset_uuid.as_string();
    auto asset_file = os::MappedFile::open(asset_path.string()).value();

    const char *file_identifier = reinterpret_cast<const char*>(exo::ptr_offset(asset_file.base_addr, sizeof(u64)));
    auto &asset_ctors = global_asset_constructors();
    auto *new_asset = asset_ctors.create({file_identifier, 4});
    if (!new_asset)
    {
        return Err(AssetErrors::NoLoaderFound, asset_uuid);
    }
    this->create_asset_internal(new_asset, asset_uuid);

    exo::ScopeStack scope       = exo::ScopeStack::with_allocator(&exo::tls_allocator);
    exo::Serializer serializer  = exo::Serializer::create(&scope);
    serializer.buffer      = const_cast<void *>(asset_file.base_addr);
    serializer.buffer_size = asset_file.size;
    serializer.is_writing  = false;
    new_asset->serialize(serializer);
    new_asset->state = AssetState::Loaded;

    for (auto dependency_uuid : new_asset->dependencies)
    {
        this->load_asset(dependency_uuid);
    }

    return Ok(new_asset);
}

void AssetManager::unload_asset(os::UUID asset_uuid)
{
    ASSERT(assets.contains(asset_uuid));
    assets.erase(asset_uuid);
}

Result<Asset*> AssetManager::load_or_import_resource(os::UUID resource_uuid)
{
    // TODO: should import if the the asset has been corrupted?

    if (resource_metadatas.contains(resource_uuid) == false)
    {
        return Err(AssetErrors::InvalidUUID, resource_uuid);
    }

    auto &resource_meta = resource_metadatas.at(resource_uuid);
    auto  load          = exo::leaf::on_error(exo::leaf::e_file_name{resource_meta.resource_path.string()});
    auto  resource_file = os::MappedFile::open(resource_meta.resource_path.string()).value();
    u64   file_hash     = hash_file(resource_file.base_addr, resource_file.size);

    auto asset_path = assets_directory / resource_uuid.as_string();

    if (resource_meta.last_imported_hash == file_hash && std::filesystem::exists(asset_path))
    {
        return load_asset(resource_uuid);
    }
    else
    {
        return import_resource(resource_uuid);
    }
}

// --

Result<u32> AssetManager::find_importer(const void *data, usize len)
{
    u32 i_found_importer = u32_invalid;
    for (u32 i_importer = 0; i_importer < importers.size(); i_importer += 1)
    {
        if (importers[i_importer].can_import(data, len))
        {
            i_found_importer = i_importer;
            break;
        }
    }

    if (i_found_importer == u32_invalid)
    {
        return Err(AssetErrors::NoImporterFound);
    }

    return Ok(i_found_importer);
}

// -- Metadata files

bool AssetManager::has_meta_file(const std::filesystem::path &file_path)
{
    constexpr static bool OVERWRITE_META = false;
    auto                  meta_path      = resource_path_to_meta_path(file_path);
    auto                  has_meta_file  = std::filesystem::is_regular_file(meta_path);
    return has_meta_file == true && OVERWRITE_META == false;
}

Result<os::UUID> AssetManager::create_resource_meta(const std::filesystem::path &file_path)
{
    // open file to hash its content
    auto mapped_file = os::MappedFile::open(file_path.string()).value();

    // find an importer that can import this file
    auto load = exo::leaf::on_error(exo::leaf::e_file_name{file_path.string()});
    BOOST_LEAF_AUTO(i_found_importer, find_importer(mapped_file.base_addr, mapped_file.size));
    auto &importer = importers[i_found_importer];

    // create new meta in the map
    auto          uuid     = os::UUID::create();
    ResourceMeta &new_meta = resource_metadatas[uuid];
    new_meta.uuid          = uuid;
    new_meta.resource_path = std::move(file_path);
    new_meta.meta_path     = resource_path_to_meta_path(file_path);
    new_meta.importer_data = importer.create_default_importer_data();
    new_meta.last_imported_hash = 0;

    save_resource_meta(importer, new_meta);

    exo::logger::info("[AssetManager] Created metadata for resource {}\n", file_path);
    return Ok(uuid);
}


Result<void> AssetManager::save_resource_meta(GenericImporter &importer, ResourceMeta &meta)
{
    // write it to filesystem
    exo::logger::info("[AssetManager] Writing meta file {}\n", meta.meta_path);
    FILE *                     fp = fopen(meta.meta_path.string().c_str(), "wb"); // non-Windows use "w"
    char                       buffer[65536];
    rapidjson::FileWriteStream os(fp, buffer, sizeof(buffer));
    rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);
    writer.StartObject();
    writer.Key("uuid");
    writer.String(meta.uuid.str, os::UUID::STR_LEN);
    writer.Key("display_name");
    writer.String(meta.display_name, strlen(meta.display_name));
    writer.Key("resource_path");
    writer.String(meta.resource_path.string().c_str(), static_cast<u32>(meta.resource_path.string().size()));
    writer.Key("meta_path");
    writer.String(meta.meta_path.string().c_str(), static_cast<u32>(meta.meta_path.string().size()));
    writer.Key("last_imported_hash");
    writer.Uint64(meta.last_imported_hash);
    writer.Key("importer_data");
    importer.write_data_json(writer, meta.importer_data);
    writer.EndObject();
    fclose(fp);

    return Ok<void>();
}

Result<os::UUID> AssetManager::load_resource_meta(GenericImporter &importer, const std::filesystem::path &file_path)
{
    auto meta_path   = resource_path_to_meta_path(file_path);
    auto mapped_file = os::MappedFile::open(meta_path.string()).value();

    std::string_view    file_content{reinterpret_cast<const char *>(mapped_file.base_addr), mapped_file.size};
    rapidjson::Document document;
    document.Parse(file_content.data(), file_content.size());
    if (document.HasParseError())
    {
        return Err(AssetErrors::ParsingError,
                   JsonError{document.GetErrorOffset(), rapidjson::GetParseError_En(document.GetParseError())});
    }

    // Deserialize it
    const char *uuid_str = document["uuid"].GetString();
    os::UUID uuid     = os::UUID::from_string(uuid_str, strlen(uuid_str));

    const char *display_name_str   = document["display_name"].GetString();
    const char *resource_path_str  = document["resource_path"].GetString();
    const char *meta_path_str      = document["meta_path"].GetString();
    u64         last_imported_hash = document["last_imported_hash"].GetUint64();

    ResourceMeta &new_meta      = resource_metadatas[uuid];
    new_meta.display_name       = exo::tls_string_repository.intern(display_name_str);
    new_meta.uuid               = uuid;
    new_meta.resource_path      = std::string(resource_path_str);
    new_meta.meta_path          = std::string(meta_path_str);
    new_meta.last_imported_hash = last_imported_hash;
    new_meta.importer_data      = importer.read_data_json(document["importer_data"]);

    exo::logger::info("[AssetManager] Loaded metadata for resource {}.\n", uuid_str);

    return Ok(uuid);
}

Result<AssetMeta&> AssetManager::create_asset_meta(os::UUID uuid)
{
    auto load = exo::leaf::on_error(uuid);

    // create new meta in the map
    AssetMeta &new_meta   = asset_metadatas[uuid];
    new_meta.uuid         = uuid;
    new_meta.display_name = "unnamed";
    new_meta.asset_hash   = 0;

    save_asset_meta(new_meta);

    exo::logger::info("[AssetManager] Created metadata for asset {:.{}}\n", uuid.str, os::UUID::STR_LEN);
    return Ok(new_meta);
}

Result<void> AssetManager::save_asset_meta(AssetMeta &meta)
{
    exo::logger::info("[AssetManager] Writing asset meta file {}\n", meta.uuid);

    auto meta_path = assets_directory / meta.uuid.as_string();
    meta_path += ".meta";

    FILE *                     fp = fopen(meta_path.string().c_str(), "wb"); // non-Windows use "w"
    char                       buffer[65536];
    rapidjson::FileWriteStream os(fp, buffer, sizeof(buffer));
    rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);
    writer.StartObject();
    writer.Key("uuid");
    writer.String(meta.uuid.str, os::UUID::STR_LEN);
    writer.Key("display_name");
    writer.String(meta.display_name, strlen(meta.display_name));
    writer.Key("asset_hash");
    writer.Uint64(meta.asset_hash);
    writer.EndObject();
    fclose(fp);

    return Ok<void>();
}

Result<AssetMeta&> AssetManager::load_asset_meta(os::UUID uuid)
{
    auto meta_path   = assets_directory / uuid.as_string();
    meta_path += ".meta";

    auto mapped_file = os::MappedFile::open(meta_path.string()).value();

    std::string_view    file_content{reinterpret_cast<const char *>(mapped_file.base_addr), mapped_file.size};
    rapidjson::Document document;
    document.Parse(file_content.data(), file_content.size());
    if (document.HasParseError())
    {
        return Err(AssetErrors::ParsingError,
                   JsonError{document.GetErrorOffset(), rapidjson::GetParseError_En(document.GetParseError())});
    }

    // Deserialize it
    const char *uuid_str = document["uuid"].GetString();
    ASSERT(os::UUID::from_string(uuid_str, strlen(uuid_str)) == uuid);

    const char *display_name_str = document["display_name"].GetString();
    auto        asset_hash       = document["asset_hash"].GetUint64();

    AssetMeta &new_meta   = asset_metadatas[uuid];
    new_meta.display_name = exo::tls_string_repository.intern(display_name_str);
    new_meta.uuid         = uuid;
    new_meta.asset_hash   = asset_hash;

    exo::logger::info("[AssetManager] Loaded metadata for resource {}.\n", uuid_str);

    return Ok(new_meta);
}
