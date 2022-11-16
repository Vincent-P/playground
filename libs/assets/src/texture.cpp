#include "assets/texture.h"

#include "exo/profile.h"
#include "exo/serialization/u128_serializer.h"

namespace exo
{
void serialize(Serializer &serializer, PixelFormat &data)
{
	auto value = static_cast<std::underlying_type_t<PixelFormat>>(data);
	serialize(serializer, value);
	if (serializer.is_writing == false) {
		data = static_cast<PixelFormat>(value);
	}
}

void serialize(Serializer &serializer, ImageExtension &data)
{
	auto value = static_cast<std::underlying_type_t<ImageExtension>>(data);
	serialize(serializer, value);
	if (serializer.is_writing == false) {
		data = static_cast<ImageExtension>(value);
	}
}
} // namespace exo

void Texture::serialize(exo::Serializer &serializer)
{
	Asset::serialize(serializer);
	exo::serialize(serializer, this->format);
	exo::serialize(serializer, this->extension);
	exo::serialize(serializer, this->width);
	exo::serialize(serializer, this->height);
	exo::serialize(serializer, this->depth);
	exo::serialize(serializer, this->levels);
	exo::serialize(serializer, this->mip_offsets);
	exo::serialize(serializer, this->pixels_hash);
	exo::serialize(serializer, this->pixels_data_size);
}
