#include "assets/texture.h"
#include "assets/asset_constructors.h"

#include <exo/profile.h>

static int texture_ctor = global_asset_constructors().add_constructor(get_asset_id<Texture>(), &Texture::create);

Asset *Texture::create() { return new Texture(); }

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
	const char *id = "TXTR";
	exo::serialize(serializer, id);
	Asset::serialize(serializer);
	exo::serialize(serializer, this->format);
	exo::serialize(serializer, this->extension);
	exo::serialize(serializer, this->width);
	exo::serialize(serializer, this->height);
	exo::serialize(serializer, this->depth);
	exo::serialize(serializer, this->levels);
	exo::serialize(serializer, this->mip_offsets);

	exo::serialize(serializer, this->data_size);
	if (serializer.is_writing) {
		serializer.write_bytes(this->pixels_data, this->data_size);
	} else {
		this->impl_data = malloc(this->data_size);
		EXO_PROFILE_MALLOC(this->impl_data, this->data_size);
		serializer.read_bytes(this->impl_data, this->data_size);
		this->pixels_data = this->impl_data;
	}
}
