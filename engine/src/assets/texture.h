#pragma once
#include <exo/maths/numerics.h>
#include <exo/collections/vector.h>

enum struct ImageExtension
{
    KTX2,
    PNG
};

enum struct PixelFormat
{
    R8G8B8A8_UNORM,
    R8G8B8A8_SRGB,
    BC4_UNORM, // one channel
    BC5_UNORM, // two channels
    BC7_UNORM, // 4 channels
    BC7_SRGB,  // 4 channels
};

struct Texture
{
    static Texture from_raw_pixels(const void *data, usize len, PixelFormat format);
    static Texture from_png(const void *data, usize len);
    static Texture from_ktx2(const void *data, usize len);

    void *ktx_texture;
    Vec<u8> png_pixels;

    PixelFormat format;
    ImageExtension extension;
    i32 width;
    i32 height;
    i32 depth;
    i32 levels;

    Vec<usize> mip_offsets;
    const void* raw_data;
    usize data_size;
};
