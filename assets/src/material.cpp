#include "assets/material.h"
#include "assets/asset_constructors.h"

#include <exo/serializer.h>

static int material_ctor = global_asset_constructors().add_constructor("MTRL", &Material::create);

Asset *Material::create() { return new Material(); }

void Material::serialize(exo::Serializer &serializer)
{
	const char *id = "MTRL";
	serializer.serialize(id);
	Asset::serialize(serializer);
	serializer.serialize(this->base_color_factor);
	serializer.serialize(this->emissive_factor);
	serializer.serialize(this->metallic_factor);
	serializer.serialize(this->roughness_factor);
	serializer.serialize(this->base_color_texture);
	serializer.serialize(this->normal_texture);
	serializer.serialize(this->metallic_roughness_texture);
	serializer.serialize(this->uv_transform.offset);
	serializer.serialize(this->uv_transform.scale);
	serializer.serialize(this->uv_transform.rotation);
}
