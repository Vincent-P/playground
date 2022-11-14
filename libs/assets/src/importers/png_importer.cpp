#include "assets/importers/png_importer.h"

#include <assets/asset_id.h>
#include <exo/macros/defer.h>
#include <exo/profile.h>

#include <cross/mapped_file.h>

#include "assets/asset_id_formatter.h"
#include "assets/asset_manager.h"
#include "assets/texture.h"

#include <spng.h>

bool PNGImporter::can_import_extension(exo::Span<const exo::StringView> extensions)
{
	for (const auto &extension : extensions) {
		if (extension == exo::StringView{".png"}) {
			return true;
		}
	}
	return false;
}

bool PNGImporter::can_import_blob(exo::Span<const u8> blob)
{
	const u8 signature[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

	ASSERT(blob.len() > sizeof(signature));
	return std::memcmp(blob.data(), signature, sizeof(signature)) == 0;
}

Result<CreateResponse> PNGImporter::create_asset(const CreateRequest &request)
{
	CreateResponse response{};
	if (request.asset.is_valid()) {
		response.new_id = request.asset;
	} else {
		response.new_id = AssetId::create<Texture>(request.path.filename());
	}

	return Ok(std::move(response));
}

Result<ProcessResponse> PNGImporter::process_asset(const ProcessRequest &request)
{
	ASSERT(request.asset.is_valid());

	spng_ctx *ctx = spng_ctx_new(0);
	DEFER { spng_ctx_free(ctx); };

	auto file = cross::MappedFile::open(request.path.view()).value();
	auto blob = file.content();

	spng_set_png_buffer(ctx, blob.data(), blob.len());

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
	const usize bytes_per_pixel = (decoded_size / usize(ihdr.width)) / usize(ihdr.height);

	// dont support 16 bits for now
	ASSERT(ihdr.bit_depth == 8);

	ASSERT(((8 * bytes_per_pixel) % ihdr.bit_depth) == 0);
	const u32 channel_count = u32((8 * bytes_per_pixel) / u32(ihdr.bit_depth));
	auto      pixel_format  = PixelFormat::R8G8B8A8_UNORM;
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

	u8 *buffer = reinterpret_cast<u8 *>(malloc(decoded_size));
	EXO_PROFILE_MALLOC(buffer, decoded_size);
	if (spng_decode_image(ctx, buffer, decoded_size, fmt, 0)) {
		return Err<Asset *>(PNGErrors::CannotDecodeSize);
	}

	auto  asset_id         = request.asset;
	auto *new_texture      = request.importer_api.create_asset<Texture>(request.asset);
	new_texture->name      = request.asset.name;
	new_texture->extension = ImageExtension::PNG;
	new_texture->width     = static_cast<int>(ihdr.width);
	new_texture->height    = static_cast<int>(ihdr.height);
	new_texture->depth     = 1;
	new_texture->levels    = 1;
	new_texture->format    = pixel_format;
	new_texture->mip_offsets.push(0);
	new_texture->pixels_data_size = decoded_size;
	new_texture->pixels_hash      = request.importer_api.save_blob(exo::Span(buffer, decoded_size));
	new_texture->pixels_data_size = decoded_size;

	EXO_PROFILE_MFREE(buffer);
	free(buffer);

	Vec<AssetId> products;
	products.push(std::move(asset_id));
	return Ok(ProcessResponse{.products = std::move(products)});
}
