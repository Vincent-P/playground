#pragma once
#include <exo/maths/numerics.h>
#include <exo/collections/vector.h>
#include <exo/serializer.h>

#include "assets/asset.h"

enum struct ImageExtension : u16
{
  KTX2,
  PNG
};

enum struct PixelFormat : u16
{
    R8G8B8A8_UNORM,
    R8G8B8A8_SRGB,
    BC4_UNORM, // one channel
    BC5_UNORM, // two channels
    BC7_UNORM, // 4 channels
    BC7_SRGB,  // 4 channels
};

struct Texture : Asset
{
    const char *type_name() const final { return "Texture"; }
    void serialize(Serializer& serializer) final;

    PixelFormat format;
    ImageExtension extension;
    i32 width;
    i32 height;
    i32 depth;
    i32 levels;
    Vec<usize> mip_offsets;

    void *impl_data; // ktxTexture* for libktx, u8* containing raw pixels for png
    const void* pixels_data;
    usize data_size;
};

template<>
void Serializer::serialize<Texture>(Texture &data);

template<>
void Serializer::serialize<PixelFormat>(PixelFormat &data);

template<>
void Serializer::serialize<ImageExtension>(ImageExtension &data);
