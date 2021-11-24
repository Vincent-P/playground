#include "assets/texture.h"

#include <exo/collections/vector.h>
#include "assets/flatbuffer_utils.h"
#include "schemas/texture_generated.h"

void Texture::from_flatbuffer(const void *data, usize /*len*/)
{
    ASSERT(engine::schemas::TextureBufferHasIdentifier(data));
    const auto *texture_buffer = engine::schemas::GetTexture(data);

    from_asset(texture_buffer->asset(), this);

    this->format    = texture_buffer->format();
    this->extension = texture_buffer->extension();
    this->width     = texture_buffer->width();
    this->height    = texture_buffer->height();
    this->depth     = texture_buffer->depth();
    this->levels    = texture_buffer->levels();

    const auto *texture_mip_offsets = texture_buffer->mip_offset();
    this->mip_offsets = Vec<usize>(texture_mip_offsets->data(), texture_mip_offsets->data() + texture_mip_offsets->size());

    this->data_size = texture_buffer->data_size();

    const auto *texture_data = texture_buffer->data();
    this->impl_data          = malloc(this->data_size);
    std::memcpy(this->impl_data, texture_data, this->data_size);
    this->raw_data = this->impl_data;
}

void Texture::to_flatbuffer(flatbuffers::FlatBufferBuilder &builder, u32 &o_offset, u32 &o_size) const
{
    auto asset_offset       = to_asset(this, builder);
    auto mip_offsets_offset = builder.CreateVectorScalarCast<usize>(this->mip_offsets.data(), this->mip_offsets.size());
    auto data_offset = builder.CreateVectorScalarCast<i8>(reinterpret_cast<const i8 *>(this->raw_data), this->data_size);

    engine::schemas::TextureBuilder texture_builder{builder};
    texture_builder.add_asset(asset_offset);
    texture_builder.add_format(this->format);
    texture_builder.add_extension(this->extension);
    texture_builder.add_width(this->width);
    texture_builder.add_height(this->height);
    texture_builder.add_depth(this->depth);
    texture_builder.add_levels(this->levels);
    texture_builder.add_mip_offset(mip_offsets_offset);
    texture_builder.add_data(data_offset);
    texture_builder.add_data_size(this->data_size);

    auto texture_offset = texture_builder.Finish();
    // builder.Finish() doesn't add a file identifier
    engine::schemas::FinishTextureBuffer(builder, texture_offset);

    o_offset = texture_offset.o;
    o_size   = builder.GetSize();
}
