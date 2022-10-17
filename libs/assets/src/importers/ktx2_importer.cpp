#include "assets/importers/ktx2_importer.h"

#include <exo/macros/defer.h>

#include "assets/asset_id_formatter.h"
#include "assets/asset_manager.h"
#include "assets/texture.h"

#include <ktx.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include <volk.h>

bool KTX2Importer::can_import_extension(std::span<const std::string_view> extensions)
{
	for (const auto extension : extensions) {
		if (extension == std::string_view{".ktx2"}) {
			return true;
		}
	}
	return false;
}

bool KTX2Importer::can_import_blob(std::span<const u8> blob)
{
	const u8 signature[] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};

	ASSERT(blob.size() > sizeof(signature));
	return std::memcmp(blob.data(), signature, sizeof(signature)) == 0;
}

Result<CreateResponse> KTX2Importer::create_asset(const CreateRequest & /*request*/) { return Ok(CreateResponse{}); }

Result<ProcessResponse> KTX2Importer::process_asset(const ProcessRequest & /*request*/)
{
	return Ok(ProcessResponse{});
}

#if 0
Result<Asset *> KTX2Importer::import(ImporterApi &api, exo::UUID resource_uuid, std::span<u8 const> blob)
{
	ktxTexture2 *ktx_texture = nullptr;
	auto         result =
		ktxTexture2_CreateFromMemory(blob.data(), blob.size(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx_texture);
	if (result != KTX_SUCCESS) {
		return Err<Asset *>(KTX2Errors::CreateFailed);
	}

	auto *new_texture = new Texture();
	new_texture->uuid = resource_uuid;

	// https://github.com/KhronosGroup/3D-Formats-Guidelines/blob/main/KTXDeveloperGuide.md
	if (ktxTexture2_NeedsTranscoding(ktx_texture)) {
		ktx_transcode_fmt_e target_format = KTX_TTF_BC7_RGBA;

		u32 nb_components = ktxTexture2_GetNumComponents(ktx_texture);
		if (ktx_texture->supercompressionScheme == KTX_SS_BASIS_LZ) {
			if (nb_components == 1) {
				target_format = KTX_TTF_BC4_R;
			} else if (nb_components == 2) {
				target_format = KTX_TTF_BC5_RG;
			}
		}

		result = ktxTexture2_TranscodeBasis(ktx_texture, target_format, 0);
		if (result != KTX_SUCCESS) {
			ktxTexture_Destroy(reinterpret_cast<ktxTexture *>(ktx_texture));
			ktx_texture = nullptr;
			return Err<Asset *>(KTX2Errors::TranscodeFailed);
		}
	}

	new_texture->impl_data = ktx_texture;
	new_texture->format    = from_vk(static_cast<VkFormat>(ktx_texture->vkFormat));
	new_texture->extension = ImageExtension::KTX2;
	new_texture->width     = static_cast<int>(ktx_texture->baseWidth);
	new_texture->height    = static_cast<int>(ktx_texture->baseHeight);
	new_texture->depth     = static_cast<int>(ktx_texture->baseDepth);
	new_texture->levels    = static_cast<int>(ktx_texture->numLevels);

	for (int i = 0; i < new_texture->levels; i += 1) {
		new_texture->mip_offsets.push_back(0);
		result = ktxTexture_GetImageOffset(reinterpret_cast<ktxTexture *>(ktx_texture),
			i,
			0,
			0,
			&new_texture->mip_offsets.back());
		ASSERT(result == KTX_SUCCESS);
	}

	new_texture->pixels_data = ktx_texture->pData;
	new_texture->data_size   = ktx_texture->dataSize;

	return Ok<Asset *>(new_texture);
}
#endif
