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
    static Asset *create();
    const char *type_name() const final { return "Texture"; }
    void serialize(exo::Serializer& serializer) final;
    void display_ui() final {}

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
void exo::Serializer::serialize<Texture>(Texture &data);

template<>
void exo::Serializer::serialize<PixelFormat>(PixelFormat &data);

template<>
void exo::Serializer::serialize<ImageExtension>(ImageExtension &data);
