#pragma once
#include <exo/collections/vector.h>
#include <exo/maths/numerics.h>
#include <exo/maths/u128.h>
#include <exo/serialization/serializer.h>
#include <exo/uuid.h>

#include "assets/asset.h"
#include "assets/asset_id.h"

#include <span>

enum struct ImageExtension : u16
{
	KTX2,
	PNG
};

enum struct PixelFormat : u16
{
	R8_UNORM,
	R8G8_UNORM,
	R8G8B8_UNORM,
	R8G8B8_SRGB,
	R8G8B8A8_UNORM,
	R8G8B8A8_SRGB,
	BC4_UNORM, // one channel
	BC5_UNORM, // two channels
	BC7_UNORM, // 4 channels
	BC7_SRGB,  // 4 channels
};

REGISTER_ASSET_TYPE(Texture, create_asset_id('TEX'))
struct Texture : Asset
{
	static Asset *create();
	const char   *type_name() const final { return "Texture"; }
	void          serialize(exo::Serializer &serializer) final;

	PixelFormat    format;
	ImageExtension extension;
	i32            width;
	i32            height;
	i32            depth;
	i32            levels;
	Vec<usize>     mip_offsets;

	exo::u128 pixels_hash;
	usize     pixels_data_size;
};

namespace exo
{
void serialize(Serializer &serializer, PixelFormat &data);
void serialize(Serializer &serializer, ImageExtension &data);
} // namespace exo
