#include "assets/asset_manager.h"

#include "assets/asset.h"
#include "assets/asset_constructors.h"
#include "assets/importers/gltf_importer.h"
#include "assets/importers/ktx2_importer.h"
#include "assets/importers/png_importer.h"

#include <exo/logger.h>
#include <exo/memory/scope_stack.h>
#include <exo/memory/string_repository.h>
#include <exo/result.h>
#include <exo/uuid.h>

#include <cross/file_watcher.h>
#include <cross/mapped_file.h>

#include <cstdio>
#include <filesystem>
#include <meow_hash_x64_aesni.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include <string>

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
	auto  meow_hash      = MeowHash(MeowDefaultSeed, len, non_const_data);
	return static_cast<u64>(_mm_extract_epi64(meow_hash, 0));
}
} // namespace

// -- Asset Manager

AssetManager *AssetManager::create(exo::ScopeStack &scope)
{
	auto *asset_manager = scope.allocate<AssetManager>();

	asset_manager->importers.push_back(GLTFImporter{});
	asset_manager->importers.push_back(PNGImporter{});
	asset_manager->importers.push_back(KTX2Importer{});

	return asset_manager;
}

AssetManager::~AssetManager() {}

void AssetManager::load_all_metas()
{
	auto error_handler = [](exo::ErrorWrapper error) {
		auto error_enum = static_cast<AssetErrors>(error.code);
		switch (error_enum) {
		case AssetErrors::NoImporterFound: {
			exo::logger::error("[AssetManager] No importer found\n");
			break;
		}
		case AssetErrors::NoLoaderFound: {
			exo::logger::error("[AssetManager] No loader found\n");
			break;
		}
		case AssetErrors::ParsingError: {
			exo::logger::error("[AssetManager] Parsing error\n");
			break;
		}
		case AssetErrors::InvalidUUID: {
			exo::logger::error("[AssetManager] Invalid UUID\n");
			break;
		}
		default: {
			exo::logger::error("[AssetManager] Unknown error {}\n", error.code);
			break;
		}
		};
	};

	// Load or create all resources meta
	for (const auto &file_entry : std::filesystem::recursive_directory_iterator{this->resources_directory}) {
		if (file_entry.is_regular_file() == false) {
			continue;
		}

		const auto &file_path = file_entry.path();

		Result<exo::UUID> new_uuid = Err<exo::UUID>(42);
		if (this->has_meta_file(file_path)) {
			auto resource_file  = cross::MappedFile::open(file_path.string()).value();
			auto i_importer_res = this->find_importer(resource_file.base_addr, resource_file.size);
			if (i_importer_res) {
				auto i_importer = i_importer_res.value();
				new_uuid        = this->load_resource_meta(importers[i_importer], file_path);
			} else {
				// propagate error
				new_uuid = std::move(i_importer_res);
			}
		} else {
			new_uuid = this->create_resource_meta(file_path);
		}

		if (!new_uuid) {
			error_handler(new_uuid.error());
		}
	}

	exo::logger::info("[AssetManager] Done checking in all resources.\n");

	// process all assets
	for (const auto &file_entry : std::filesystem::recursive_directory_iterator{this->assets_directory}) {
		if (file_entry.is_regular_file() == false) {
			continue;
		}
		auto filename = file_entry.path().filename().string();
		if (filename.size() != exo::UUID::STR_LEN) {
			continue;
		}

		auto uuid = exo::UUID::from_string(filename);
		exo::logger::info("[AssetManager] Found asset {}.\n", filename);

		Result<AssetMeta *> new_meta = Err<AssetMeta *>(42);
		if (this->has_meta_file(file_entry.path())) {
			new_meta = this->load_asset_meta(uuid);
		} else {
			new_meta = this->create_asset_meta(uuid);
		}
		if (!new_meta) {
			error_handler(new_meta.error());
		}
	}
}

void AssetManager::setup_file_watcher(cross::FileWatcher &watcher)
{
	int assets_wd = watcher.add_watch(ASSET_PATH).wd;

	// TODO: Properly watch file system changes and respond accordingly
	watcher.on_file_change([&, assets_wd](const cross::Watch &watch, const cross::WatchEvent &event) {
		if (watch.wd != assets_wd) {
			return;
		}

		const char *p = "";
		switch (event.action) {
		case cross::WatchEventAction::FileChanged:
			p = "file changed: ";
			break;
		case cross::WatchEventAction::FileRemoved:
			p = "file removed: ";
			break;
		case cross::WatchEventAction::FileAdded:
			p = "file added: ";
			break;
		case cross::WatchEventAction::FileRenamed:
			p = "file renamed: ";
			break;
		}

		auto file_path = this->resources_directory / event.name;

		exo::logger::info("[AssetManager] {} {}\n", p, file_path);

		if (event.action == cross::WatchEventAction::FileChanged ||
			event.action == cross::WatchEventAction::FileAdded) {
			if (!this->has_meta_file(file_path)) {
				this->create_resource_meta(file_path);
			}
		}

		if (event.action == cross::WatchEventAction::FileRemoved) {
			// unload asset, remove meta from memory and filesystem
		}

		if (event.action == cross::WatchEventAction::FileRenamed) {
		}
	});
}

Result<Asset *> AssetManager::import_resource(
	const void *data, usize len, void *importer_data, u32 i_importer, exo::UUID resource_uuid)
{
	if (i_importer == u32_invalid) {
		auto res = find_importer(data, len);
		if (res) {
			i_importer = res.value();
		} else {
			return res;
		}
	}
	return importers[i_importer].import(this, resource_uuid, data, len, importer_data);
}

Result<Asset *> AssetManager::import_resource(exo::UUID resource_uuid)
{
	// Invalid UUID?
	ASSERT(resource_metadatas.contains(resource_uuid));
	// exo::logger::info("[AssetManager] importing resource {} from disk\n", resource_uuid);

	auto &resource_meta = resource_metadatas.at(resource_uuid);
	auto  resource_file = cross::MappedFile::open(resource_meta.resource_path.string()).value();

	u64 file_hash = hash_file(resource_file.base_addr, resource_file.size);

	// TODO: only check if debug checks are enabled
	// ASSERT(resource_meta.last_imported_hash != file_hash);

	auto i_importer = find_importer(resource_file.base_addr, resource_file.size);
	if (!i_importer)
		return i_importer;

	auto new_asset = this->import_resource(resource_file.base_addr,
		resource_file.size,
		resource_meta.importer_data,
		i_importer.value(),
		resource_uuid);
	if (!new_asset)
		return new_asset;

	resource_meta.last_imported_hash = file_hash;
	auto res                         = save_resource_meta(importers[i_importer.value()], resource_meta);
	if (!res)
		return res;

	return new_asset;
}

// -- Asset files
Result<Asset *> AssetManager::get_asset(exo::UUID asset_uuid)
{
	if (assets.contains(asset_uuid)) {
		return Ok(assets.at(asset_uuid));
	}
	return Err<Asset *>(AssetErrors::InvalidUUID);
}

void AssetManager::create_asset_internal(Asset *asset, exo::UUID uuid)
{
	if (!uuid.is_valid()) {
		uuid = exo::UUID::create();
	}
	ASSERT(assets.contains(uuid) == false);
	ASSERT(uuid.is_valid());

	assets[uuid] = asset;
	asset->uuid  = uuid;
}

Result<void> AssetManager::save_asset(Asset *asset)
{
	exo::ScopeStack scope      = exo::ScopeStack::with_allocator(&exo::tls_allocator);
	exo::Serializer serializer = exo::Serializer::create(&scope);
	serializer.buffer_size     = 32_MiB;
	serializer.buffer          = scope.allocate(serializer.buffer_size);
	serializer.is_writing      = true;
	asset->serialize(serializer);

	// Save asset to disk
	auto  asset_path = assets_directory / asset->uuid.as_string();
	FILE *fp         = fopen(asset_path.string().c_str(), "wb"); // non-Windows use "w"
	auto  bwritten   = fwrite(serializer.buffer, 1, serializer.offset, fp);
	ASSERT(bwritten == serializer.offset);

	fclose(fp);

	// Load or create asset meta
	if (has_meta_file(asset_path)) {
		auto res = load_asset_meta(asset->uuid);
		if (!res) {
			return res;
		}
	} else {
		auto res = create_asset_meta(asset->uuid);
		if (!res) {
			return res;
		}
	}

	return Ok();
}

Result<Asset *> AssetManager::load_asset(exo::UUID asset_uuid)
{
	if (assets.contains(asset_uuid)) {
		// exo::logger::info("[AssetManager] loading asset {} from memory\n", asset_uuid);
		return Ok(assets.at(asset_uuid));
	}
	// exo::logger::info("[AssetManager] loading asset {} from disk\n", asset_uuid);

	auto asset_path = assets_directory / asset_uuid.as_string();
	auto asset_file = cross::MappedFile::open(asset_path.string()).value();

	const char *file_identifier = reinterpret_cast<const char *>(exo::ptr_offset(asset_file.base_addr, sizeof(u64)));
	auto       &asset_ctors     = global_asset_constructors();
	auto       *new_asset       = asset_ctors.create({file_identifier, 4});
	if (!new_asset) {
		return Err<Asset *>(AssetErrors::NoLoaderFound);
	}
	this->create_asset_internal(new_asset, asset_uuid);

	exo::ScopeStack scope      = exo::ScopeStack::with_allocator(&exo::tls_allocator);
	exo::Serializer serializer = exo::Serializer::create(&scope);
	serializer.buffer          = const_cast<void *>(asset_file.base_addr);
	serializer.buffer_size     = asset_file.size;
	serializer.is_writing      = false;
	new_asset->serialize(serializer);
	new_asset->state = AssetState::Loaded;

	for (auto dependency_uuid : new_asset->dependencies) {
		this->load_asset(dependency_uuid);
	}

	return Ok(new_asset);
}

void AssetManager::unload_asset(exo::UUID asset_uuid)
{
	ASSERT(assets.contains(asset_uuid));
	assets.erase(asset_uuid);
}

Result<Asset *> AssetManager::load_or_import_resource(exo::UUID resource_uuid)
{
	// TODO: should import if the the asset has been corrupted?

	if (resource_metadatas.contains(resource_uuid) == false) {
		return Err<Asset *>(AssetErrors::InvalidUUID);
	}

	auto &resource_meta = resource_metadatas.at(resource_uuid);
	auto  resource_file = cross::MappedFile::open(resource_meta.resource_path.string()).value();
	u64   file_hash     = hash_file(resource_file.base_addr, resource_file.size);

	auto asset_path = assets_directory / resource_uuid.as_string();

	if (resource_meta.last_imported_hash == file_hash && std::filesystem::exists(asset_path)) {
		return load_asset(resource_uuid);
	} else {
		return import_resource(resource_uuid);
	}
}

// --

Result<u32> AssetManager::find_importer(const void *data, usize len)
{
	u32 i_found_importer = u32_invalid;
	for (u32 i_importer = 0; i_importer < importers.size(); i_importer += 1) {
		if (importers[i_importer].can_import(data, len)) {
			i_found_importer = i_importer;
			break;
		}
	}

	if (i_found_importer == u32_invalid) {
		return Err<u32>(AssetErrors::NoImporterFound);
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

Result<exo::UUID> AssetManager::create_resource_meta(const std::filesystem::path &file_path)
{
	// open file to hash its content
	auto mapped_file = cross::MappedFile::open(file_path.string()).value();

	// find an importer that can import this file
	auto i_found_importer_res = find_importer(mapped_file.base_addr, mapped_file.size);
	if (!i_found_importer_res)
		return i_found_importer_res;
	auto i_found_importer = i_found_importer_res.value();

	auto &importer = importers[i_found_importer];

	// create new meta in the map
	auto          uuid          = exo::UUID::create();
	ResourceMeta &new_meta      = resource_metadatas[uuid];
	new_meta.uuid               = uuid;
	new_meta.resource_path      = std::move(file_path);
	new_meta.meta_path          = resource_path_to_meta_path(file_path);
	new_meta.importer_data      = importer.create_default_importer_data();
	new_meta.last_imported_hash = 0;

	save_resource_meta(importer, new_meta);

	exo::logger::info("[AssetManager] Created metadata for resource {}\n", file_path);
	return Ok(uuid);
}

Result<void> AssetManager::save_resource_meta(GenericImporter &importer, ResourceMeta &meta)
{
	// write it to filesystem
	exo::logger::info("[AssetManager] Writing meta file {}\n", meta.meta_path);
	FILE                      *fp = fopen(meta.meta_path.string().c_str(), "wb"); // non-Windows use "w"
	char                       buffer[65536];
	rapidjson::FileWriteStream os(fp, buffer, sizeof(buffer));
	rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);
	writer.StartObject();
	writer.Key("uuid");
	writer.String(meta.uuid.str, exo::UUID::STR_LEN);
	writer.Key("display_name");
	if (meta.display_name) {

		writer.String(meta.display_name, strlen(meta.display_name));
	} else {
		writer.String("", 1);
	}
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

	return Ok();
}

Result<exo::UUID> AssetManager::load_resource_meta(GenericImporter &importer, const std::filesystem::path &file_path)
{
	auto meta_path   = resource_path_to_meta_path(file_path);
	auto mapped_file = cross::MappedFile::open(meta_path.string()).value();

	std::string_view    file_content{reinterpret_cast<const char *>(mapped_file.base_addr), mapped_file.size};
	rapidjson::Document document;
	document.Parse(file_content.data(), file_content.size());
	if (document.HasParseError()) {
		return Err<exo::UUID>(AssetErrors::ParsingError);
	}

	// Deserialize it
	const char *uuid_str = document["uuid"].GetString();
	exo::UUID   uuid     = exo::UUID::from_string(std::string_view(uuid_str));

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

Result<AssetMeta *> AssetManager::create_asset_meta(exo::UUID uuid)
{
	// create new meta in the map
	AssetMeta &new_meta   = asset_metadatas[uuid];
	new_meta.uuid         = uuid;
	new_meta.display_name = "unnamed";
	new_meta.asset_hash   = 0;

	save_asset_meta(new_meta);

	exo::logger::info("[AssetManager] Created metadata for asset {:.{}}\n", uuid.str, exo::UUID::STR_LEN);
	return Ok<AssetMeta *>(&new_meta);
}

Result<void> AssetManager::save_asset_meta(AssetMeta &meta)
{
	// exo::logger::info("[AssetManager] Writing asset meta file {}\n", meta.uuid);

	auto meta_path = assets_directory / meta.uuid.as_string();
	meta_path += ".meta";

	FILE                      *fp = fopen(meta_path.string().c_str(), "wb"); // non-Windows use "w"
	char                       buffer[65536];
	rapidjson::FileWriteStream os(fp, buffer, sizeof(buffer));
	rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);
	writer.StartObject();
	writer.Key("uuid");
	writer.String(meta.uuid.str, exo::UUID::STR_LEN);
	writer.Key("display_name");
	writer.String(meta.display_name, strlen(meta.display_name));
	writer.Key("asset_hash");
	writer.Uint64(meta.asset_hash);
	writer.EndObject();
	fclose(fp);

	return Ok();
}

Result<AssetMeta *> AssetManager::load_asset_meta(exo::UUID uuid)
{
	auto meta_path = assets_directory / uuid.as_string();
	meta_path += ".meta";

	auto mapped_file = cross::MappedFile::open(meta_path.string()).value();

	std::string_view    file_content{reinterpret_cast<const char *>(mapped_file.base_addr), mapped_file.size};
	rapidjson::Document document;
	document.Parse(file_content.data(), file_content.size());
	if (document.HasParseError()) {
		return Err<AssetMeta *>(AssetErrors::ParsingError);
	}

	// Deserialize it
	const char *uuid_str = document["uuid"].GetString();
	ASSERT(exo::UUID::from_string(std::string_view(uuid_str)) == uuid);

	const char *display_name_str = document["display_name"].GetString();
	auto        asset_hash       = document["asset_hash"].GetUint64();

	AssetMeta &new_meta   = asset_metadatas[uuid];
	new_meta.display_name = exo::tls_string_repository.intern(display_name_str);
	new_meta.uuid         = uuid;
	new_meta.asset_hash   = asset_hash;

	exo::logger::info("[AssetManager] Loaded metadata for resource {}.\n", uuid_str);

	return Ok<AssetMeta *>(&new_meta);
}
