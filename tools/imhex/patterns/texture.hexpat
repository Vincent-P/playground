#include <asset.hexpat>

enum ImageExtension : u32 {
	KTX2,
	PNG
};

enum PixelFormat : u32 {
	R8_UNORM,
	R8G8_UNORM,
	R8G8B8_UNORM,
	R8G8B8_SRGB,
	R8G8B8A8_UNORM,
	R8G8B8A8_SRGB,
	BC4_UNORM,
	BC5_UNORM,
	BC7_UNORM,
	BC7_SRGB,
};

struct Texture
{
	Asset asset;
	PixelFormat format;
	ImageExtension extension;
	s32 width;
	s32 height;
	s32 depth;
	s32 levels;
	u64 mip_len;
	u64 mip_offsets[mip_len];
	u64 pixels_data_hi;
	u64 pixels_data_lo;
	u64 pixels_data_size;
};


Texture texture @ 0x00;
