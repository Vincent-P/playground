#include "assets/texture.h"
#include "assets/asset_constructors.h"

static int texture_ctor = global_asset_constructors().add_constructor("TXTR", &Texture::create);

Asset *Texture::create()
{
    return new Texture();
}

template<>
void exo::Serializer::serialize<PixelFormat>(PixelFormat &data)
{
    auto value = static_cast<std::underlying_type_t<PixelFormat>>(data);
    serialize(value);
    if (this->is_writing == false)
    {
        data = static_cast<PixelFormat>(value);
    }
}

template<>
void exo::Serializer::serialize<ImageExtension>(ImageExtension &data)
{
    auto value = static_cast<std::underlying_type_t<ImageExtension>>(data);
    serialize(value);
    if (this->is_writing == false)
    {
        data = static_cast<ImageExtension>(value);
    }
}

void Texture::serialize(exo::Serializer& serializer)
{
    const char *id = "TXTR";
    serializer.serialize(id);
    serializer.serialize(*static_cast<Asset*>(this));
    serializer.serialize(this->format);
    serializer.serialize(this->extension);
    serializer.serialize(this->width);
    serializer.serialize(this->height);
    serializer.serialize(this->depth);
    serializer.serialize(this->levels);
    serializer.serialize(this->mip_offsets);

    serializer.serialize(this->data_size);
    if (serializer.is_writing)
    {
        serializer.write_bytes(this->pixels_data, this->data_size);
    }
    else
    {
        this->impl_data = malloc(this->data_size);
        serializer.read_bytes(this->impl_data, this->data_size);
        this->pixels_data = this->impl_data;
    }
}
