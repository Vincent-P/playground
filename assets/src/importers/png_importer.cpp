#include "assets/importers/png_importer.h"

#include <exo/logger.h>
#include <exo/macros/defer.h>

#include <cross/mapped_file.h>

#include "assets/asset_id_formatter.h"
#include "assets/asset_manager.h"
#include "assets/texture.h"

#include <spng.h>

bool PNGImporter::can_import_extension(std::span<std::string_view const> extensions)
{
	for (const auto extension : extensions) {
		if (extension == std::string_view{".png"}) {
			return true;
		}
	}
	return false;
}

bool PNGImporter::can_import_blob(std::span<u8 const> blob)
{
	const u8 signature[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

	ASSERT(blob.size() > sizeof(signature));
	return std::memcmp(blob.data(), signature, sizeof(signature)) == 0;
}

Result<CreateResponse> PNGImporter::create_asset(const CreateRequest &request)
{
	exo::logger::info("PNGImporter::create_asset({}, {})\n", request.asset, request.path.view());

	CreateResponse response{};
	if (request.asset.is_valid()) {
		response.new_id = request.asset;
	} else {
		response.new_id = AssetId::create<Texture>(request.path.filename());
	}

	return Ok(response);
}

Result<ProcessResponse> PNGImporter::process_asset(const ProcessRequest &request)
{
	exo::logger::info("PNGImporter::process_asset({}, {})\n", request.asset, request.path.view());
	ASSERT(request.asset.is_valid());

	spng_ctx *ctx = spng_ctx_new(0);
	DEFER { spng_ctx_free(ctx); };

	auto file = cross::MappedFile::open(request.path.view()).value();
	auto blob = file.content();

	spng_set_png_buffer(ctx, blob.data(), blob.size());

	spng_ihdr ihdr;
	if (spng_get_ihdr(ctx, &ihdr)) {
		return Err<Asset *>(PNGErrors::IhdrNotFound);
	}

	int fmt = SPNG_FMT_PNG;
	if (ihdr.color_type == SPNG_COLOR_TYPE_INDEXED) {
		fmt = SPNG_FMT_RGB8;
	}

	usize decoded_size = 0;
	if (spng_decoded_image_size(ctx, fmt, &decoded_size)) {
		return Err<Asset *>(PNGErrors::CannotDecodeSize);
	}

	ASSERT((decoded_size % ihdr.width) == 0);
	ASSERT(((decoded_size / ihdr.width) % ihdr.height) == 0);
	usize bytes_per_pixel = (decoded_size / usize(ihdr.width)) / usize(ihdr.height);

	// dont support 16 bits for now
	ASSERT(ihdr.bit_depth == 8);

	ASSERT(((8 * bytes_per_pixel) % ihdr.bit_depth) == 0);
	u32  channel_count = u32((8 * bytes_per_pixel) / u32(ihdr.bit_depth));
	auto pixel_format  = PixelFormat::R8G8B8A8_UNORM;
	switch (channel_count) {
	case 1: {
		pixel_format = PixelFormat::R8_UNORM;
		break;
	}
	case 2: {
		pixel_format = PixelFormat::R8G8_UNORM;
		break;
	}
	case 3: {
		// dont use R8G8B8_UNORM
		fmt          = SPNG_FMT_RGBA8;
		pixel_format = PixelFormat::R8G8B8A8_UNORM;

		if (spng_decoded_image_size(ctx, fmt, &decoded_size)) {
			return Err<Asset *>(PNGErrors::CannotDecodeSize);
		}
		break;
	}
	case 4: {
		pixel_format = PixelFormat::R8G8B8A8_UNORM;
		break;
	}
	default:
		ASSERT(false);
	}

	u8 *buffer = nullptr;
#if 1
	buffer = reinterpret_cast<u8 *>(malloc(decoded_size));
	if (spng_decode_image(ctx, buffer, decoded_size, fmt, 0)) {
		return Err<Asset *>(PNGErrors::CannotDecodeSize);
	}
#endif

	auto  asset_id         = request.asset;
	auto *new_texture      = request.importer_api.create_asset<Texture>(request.asset);
	new_texture->name      = request.asset.name;
	new_texture->extension = ImageExtension::PNG;
	new_texture->width     = static_cast<int>(ihdr.width);
	new_texture->height    = static_cast<int>(ihdr.height);
	new_texture->depth     = 1;
	new_texture->levels    = 1;
	new_texture->format    = pixel_format;
	new_texture->mip_offsets.push_back(0);
	new_texture->pixels_data = buffer;
	new_texture->data_size   = decoded_size;

	return Ok(ProcessResponse{.products = {asset_id}});
}
