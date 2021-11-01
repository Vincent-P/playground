#include "assets/texture.h"

#include <exo/prelude.h>
#include <exo/defer.h>
#include <exo/logger.h>

#include <ktx.h>
#include <spng.h>
#include <vulkan/vulkan.h>

static PixelFormat from_vk(VkFormat vk_format)
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

Texture Texture::from_ktx2(const void *data, usize len)
{
    Texture texture = {};

    ktxTexture2 *ktx_texture = nullptr;
    auto         result      = ktxTexture2_CreateFromMemory(reinterpret_cast<const u8 *>(data), len, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx_texture);

    if (result != KTX_SUCCESS)
    {
        logger::error("invalid ktx file\n");
        return {};
    }

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
            logger::error("Could not transcode the input texture to the selected target format.\n");
            ktxTexture_Destroy(reinterpret_cast<ktxTexture *>(ktx_texture));
            ktx_texture = nullptr;
            return {};
        }
    }

    texture.format    = from_vk(static_cast<VkFormat>(ktx_texture->vkFormat));
    texture.extension = ImageExtension::KTX2;
    texture.width     = static_cast<int>(ktx_texture->baseWidth);
    texture.height    = static_cast<int>(ktx_texture->baseHeight);
    texture.depth     = static_cast<int>(ktx_texture->baseDepth);
    texture.levels    = static_cast<int>(ktx_texture->numLevels);

    for (int i = 0; i < texture.levels; i += 1)
    {
        texture.mip_offsets.push_back(0);
        result = ktxTexture_GetImageOffset(reinterpret_cast<ktxTexture *>(ktx_texture), i, 0, 0, &texture.mip_offsets.back());
        ASSERT(result == KTX_SUCCESS);
    }

    texture.raw_data  = ktx_texture->pData;
    texture.data_size = ktx_texture->dataSize;

    return texture;
}

Texture Texture::from_png(const void *data, usize len)
{
    Texture texture = {};

    spng_ctx *ctx = spng_ctx_new(0);
    DEFER
    {
        spng_ctx_free(ctx);
    };

    spng_set_png_buffer(ctx, data, len);

    struct spng_ihdr ihdr;
    if (spng_get_ihdr(ctx, &ihdr))
    {
        logger::error("Could not get informations on PNG file.\n");
        return {};
    }

    usize decoded_size = 0;
    spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &decoded_size);

    texture.png_pixels = reinterpret_cast<u8*>(malloc(decoded_size));
    spng_decode_image(ctx, texture.png_pixels, decoded_size, SPNG_FMT_RGBA8, 0);

    texture.extension = ImageExtension::PNG;
    texture.width     = static_cast<int>(ihdr.width);
    texture.height    = static_cast<int>(ihdr.height);
    texture.depth     = 1;
    texture.levels    = 1;
    texture.format    = PixelFormat::R8G8B8A8_UNORM;

    texture.mip_offsets.push_back(0);

    texture.raw_data  = texture.png_pixels;
    texture.data_size = decoded_size;

    return texture;
}
