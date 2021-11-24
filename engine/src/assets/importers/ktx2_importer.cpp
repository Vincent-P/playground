#include "assets/importers/ktx2_importer.h"

#include <exo/prelude.h>
#include <exo/logger.h>
#include <exo/defer.h>

#include "assets/asset_manager.h"
#include "assets/texture.h"

#include <ktx.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/filewritestream.h>
#include <vulkan/vulkan.h>

namespace
{
PixelFormat from_vk(VkFormat vk_format)
{
    // clang-format off
    switch (vk_format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM: return PixelFormat::R8G8B8A8_UNORM;
    case VK_FORMAT_R8G8B8A8_SRGB: return PixelFormat::R8G8B8A8_SRGB;
    case VK_FORMAT_BC7_SRGB_BLOCK: return PixelFormat::BC7_SRGB;
    case VK_FORMAT_BC7_UNORM_BLOCK: return PixelFormat::BC7_UNORM;
    case VK_FORMAT_BC4_UNORM_BLOCK: return PixelFormat::BC4_UNORM;
    case VK_FORMAT_BC5_UNORM_BLOCK: return PixelFormat::BC5_UNORM;
    default: ASSERT(false);
    }
    // clang-format on
    exo::unreachable();
}
}

bool KTX2Importer::can_import(const void *file_data, usize file_len)
{
    // Section 3.1: https://github.khronos.org/KTX-Specification/
    const u8 signature[] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};

    ASSERT(file_len > sizeof(signature));
    return std::memcmp(file_data, signature, sizeof(signature)) == 0;
}

Result<Asset*> KTX2Importer::import(AssetManager *asset_manager, cross::UUID resource_uuid, const void *file_data, usize file_len, void *importer_data)
{
    auto &ktx2_importer_data = *reinterpret_cast<KTX2Importer::Data*>(importer_data);
    UNUSED(ktx2_importer_data);

    ktxTexture2 *ktx_texture = nullptr;
    auto         result      = ktxTexture2_CreateFromMemory(reinterpret_cast<const u8 *>(file_data), file_len, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx_texture);
    if (result != KTX_SUCCESS)
    {
        return Err(KTX2Errors::CreateFailed, LibKtxError{result});
    }

    auto *new_texture = asset_manager->create_asset<Texture>(resource_uuid);

    // https://github.com/KhronosGroup/3D-Formats-Guidelines/blob/main/KTXDeveloperGuide.md
    if (ktxTexture2_NeedsTranscoding(ktx_texture))
    {
        ktx_transcode_fmt_e target_format = KTX_TTF_BC7_RGBA;

        u32 nb_components = ktxTexture2_GetNumComponents(ktx_texture);
        if (ktx_texture->supercompressionScheme == KTX_SS_BASIS_LZ)
        {
            if (nb_components == 1)
            {
                target_format = KTX_TTF_BC4_R;
            }
            else if (nb_components == 2)
            {
                target_format = KTX_TTF_BC5_RG;
            }
        }

        result = ktxTexture2_TranscodeBasis(ktx_texture, target_format, 0);
        if (result != KTX_SUCCESS)
        {
            ktxTexture_Destroy(reinterpret_cast<ktxTexture *>(ktx_texture));
            ktx_texture = nullptr;
            return Err(KTX2Errors::TranscodeFailed, LibKtxError{result});
        }
    }

    new_texture->impl_data = ktx_texture;
    new_texture->format    = from_vk(static_cast<VkFormat>(ktx_texture->vkFormat));
    new_texture->extension = ImageExtension::KTX2;
    new_texture->width     = static_cast<int>(ktx_texture->baseWidth);
    new_texture->height    = static_cast<int>(ktx_texture->baseHeight);
    new_texture->depth     = static_cast<int>(ktx_texture->baseDepth);
    new_texture->levels    = static_cast<int>(ktx_texture->numLevels);

    for (int i = 0; i < new_texture->levels; i += 1)
    {
        new_texture->mip_offsets.push_back(0);
        result = ktxTexture_GetImageOffset(reinterpret_cast<ktxTexture *>(ktx_texture), i, 0, 0, &new_texture->mip_offsets.back());
        ASSERT(result == KTX_SUCCESS);
    }

    new_texture->pixels_data  = ktx_texture->pData;
    new_texture->data_size = ktx_texture->dataSize;

    asset_manager->save_asset(new_texture);
    return new_texture;
}

void *KTX2Importer::create_default_importer_data()
{
    return reinterpret_cast<void*>(new KTX2Importer::Data());
}

void *KTX2Importer::read_data_json(const rapidjson::Value &j_data)
{
    auto *new_data = create_default_importer_data();
    auto *data = reinterpret_cast<KTX2Importer::Data*>(new_data);

    const auto &j_settings = j_data["settings"].GetObj();
    data->settings = {};
    UNUSED(j_settings);

    return new_data;
}

void KTX2Importer::write_data_json(rapidjson::GenericPrettyWriter<rapidjson::FileWriteStream> &writer, const void *data)
{
    ASSERT(data);
    const auto *import_data = reinterpret_cast<const KTX2Importer::Data*>(data);
    writer.StartObject();
    UNUSED(import_data);
    writer.EndObject();
}
