#pragma once

#include <cross/uuid.h>
#include <rapidjson/fwd.h>

struct AssetManager;
struct Asset;

enum struct KTX2Errors
{
    CreateFailed,
    TranscodeFailed,
};

struct LibKtxError
{
    int error_code;
};

struct KTX2Importer
{
    struct Settings
    {
        bool do_something = false;
        bool operator==(const Settings &other) const = default;
    };

    struct Data
    {
        Settings settings;
    };

    bool can_import(const void *file_data, usize file_len);
    Result<Asset*> import(AssetManager *asset_manager, cross::UUID resource_uuid, const void *file_data, usize file_len, void *import_settings = nullptr);

    // Importer data
    void *create_default_importer_data();
    void *read_data_json(const rapidjson::Value &j_data);
    void write_data_json(rapidjson::GenericPrettyWriter<rapidjson::FileWriteStream> &writer, const void *data);
};
