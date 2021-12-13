#include "assets/importers/png_importer.h"

#include <exo/logger.h>
#include <exo/macros/defer.h>

#include "assets/asset_manager.h"
#include "assets/texture.h"

#include <spng.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/filewritestream.h>


bool PNGImporter::can_import(const void *file_data, usize file_len)
{
    const u8 signature[] = {
        0x89,
        0x50,
        0x4E,
        0x47,
        0x0D,
        0x0A,
        0x1A,
        0x0A
    };

    ASSERT(file_len > sizeof(signature));
    return std::memcmp(file_data, signature, sizeof(signature)) == 0;
}

Result<Asset*> PNGImporter::import(AssetManager *asset_manager, os::UUID resource_uuid, const void *file_data, usize file_len, void *importer_data)
{
    auto &png_importer_data = *reinterpret_cast<PNGImporter::Data*>(importer_data);
    UNUSED(png_importer_data);

    spng_ctx *ctx = spng_ctx_new(0);
    DEFER
    {
        spng_ctx_free(ctx);
    };

    spng_set_png_buffer(ctx, file_data, file_len);

    struct spng_ihdr ihdr;
    if (spng_get_ihdr(ctx, &ihdr))
    {
        return Err(PNGErrors::IhdrNotFound);
    }

    usize decoded_size = 0;
    spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &decoded_size);

    auto *new_texture      = asset_manager->create_asset<Texture>(resource_uuid);
    new_texture->impl_data = reinterpret_cast<u8 *>(malloc(decoded_size));
    spng_decode_image(ctx, new_texture->impl_data, decoded_size, SPNG_FMT_RGBA8, 0);

    new_texture->extension = ImageExtension::PNG;
    new_texture->width     = static_cast<int>(ihdr.width);
    new_texture->height    = static_cast<int>(ihdr.height);
    new_texture->depth     = 1;
    new_texture->levels    = 1;
    new_texture->format    = PixelFormat::R8G8B8A8_UNORM;
    new_texture->mip_offsets.push_back(0);

    new_texture->pixels_data  = new_texture->impl_data;
    new_texture->data_size = decoded_size;

    asset_manager->save_asset(new_texture);
    return new_texture;
}

void *PNGImporter::create_default_importer_data()
{
    return reinterpret_cast<void*>(new PNGImporter::Data());
}

void *PNGImporter::read_data_json(const rapidjson::Value &j_data)
{
    auto *new_data = create_default_importer_data();
    auto *data = reinterpret_cast<PNGImporter::Data*>(new_data);

    const auto &j_settings = j_data["settings"].GetObj();
    data->settings = {};
    UNUSED(j_settings);

    return new_data;
}

void PNGImporter::write_data_json(rapidjson::GenericPrettyWriter<rapidjson::FileWriteStream> &writer, const void *data)
{
    ASSERT(data);
    const auto *import_data = reinterpret_cast<const PNGImporter::Data*>(data);
    writer.StartObject();
    UNUSED(import_data);
    writer.EndObject();
}
