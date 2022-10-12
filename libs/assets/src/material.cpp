#include "assets/material.h"
#include "assets/asset_constructors.h"

#include <exo/serialization/serializer.h>
#include <exo/serialization/uuid_serializer.h>

static int material_ctor = global_asset_constructors().add_constructor(get_asset_id<Material>(), &Material::create);

Asset *Material::create() { return new Material(); }

void Material::serialize(exo::Serializer &serializer)
{
	const char *id = "MTRL";
	exo::serialize(serializer, id);
	Asset::serialize(serializer);
	exo::serialize(serializer, this->base_color_factor);
	exo::serialize(serializer, this->emissive_factor);
	exo::serialize(serializer, this->metallic_factor);
	exo::serialize(serializer, this->roughness_factor);
	exo::serialize(serializer, this->base_color_texture);
	exo::serialize(serializer, this->normal_texture);
	exo::serialize(serializer, this->metallic_roughness_texture);
	exo::serialize(serializer, this->uv_transform.offset);
	exo::serialize(serializer, this->uv_transform.scale);
	exo::serialize(serializer, this->uv_transform.rotation);
}
