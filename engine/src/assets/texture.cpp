#include "assets/texture.h"
#include "assets/asset_constructors.h"

static int texture_ctor = global_asset_constructors().add_constructor("TXTR", &Texture::create);

Asset *Texture::create()
{
    return new Texture();
}

void Texture::serialize(Serializer& serializer)
{
    serializer.serialize(*this);
}

template <>
void Serializer::serialize<Texture>(Texture &data)
{
    const char *id = "TXTR";
    serialize(id);
    serialize(static_cast<Asset &>(data));
    serialize(data.format);
    serialize(data.extension);
    serialize(data.width);
    serialize(data.height);
    serialize(data.depth);
    serialize(data.levels);
    serialize(data.mip_offsets);

    serialize(data.data_size);
    if (this->is_writing)
    {
        this->write_bytes(data.pixels_data, data.data_size);
    }
    else
    {
        data.impl_data = malloc(data.data_size);
        this->read_bytes(data.impl_data, data.data_size);
        data.pixels_data = data.impl_data;
    }
}

template<>
void Serializer::serialize<PixelFormat>(PixelFormat &data)
{
    auto value = static_cast<std::underlying_type_t<PixelFormat>>(data);
    serialize(value);
    if (this->is_writing == false)
    {
        data = static_cast<PixelFormat>(value);
    }
}

template<>
void Serializer::serialize<ImageExtension>(ImageExtension &data)
{
    auto value = static_cast<std::underlying_type_t<ImageExtension>>(data);
    serialize(value);
    if (this->is_writing == false)
    {
        data = static_cast<ImageExtension>(value);
    }
}
