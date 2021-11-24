#pragma once
#include <exo/maths/numerics.h>
#include <exo/collections/vector.h>

#include "schemas/texture_header_generated.h"
#include "assets/asset.h"

struct Texture : Asset
{
    const char *type_name() const final { return "Texture"; }
    void from_flatbuffer(const void *data, usize len) final;
    void to_flatbuffer(flatbuffers::FlatBufferBuilder &builder, u32 &o_offset, u32 &o_size) const final;

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
