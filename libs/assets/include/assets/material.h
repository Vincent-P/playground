#pragma once
#include "exo/maths/vectors.h"

#include "assets/asset.h"

namespace exo
{
struct Serializer;
}

struct TextureTransform
{
	float2 offset   = {0.0f, 0.0f}; // The offset of the UV coordinate origin as a factor of the texture dimensions.
	float2 scale    = {1.0f, 1.0f}; // The scale factor applied to the components of the UV coordinates.
	float  rotation = 0.0f;         // Rotate the UVs by this many radians counter-clockwise around the origin. This is
	                                // equivalent to a similar rotation of the image clockwise.

	bool operator==(const TextureTransform &other) const = default;
};

// Dependencies: Textures
struct Material : Asset
{
	using Self  = Material;
	using Super = Asset;
	REFL_REGISTER_TYPE_WITH_SUPER("Material")

	float4           base_color_factor          = float4(1.0f);
	float4           emissive_factor            = float4(0.0f);
	float            metallic_factor            = 1.0f;
	float            roughness_factor           = 1.0f;
	AssetId          base_color_texture         = {};
	AssetId          normal_texture             = {};
	AssetId          metallic_roughness_texture = {};
	TextureTransform uv_transform               = {};

	void serialize(exo::Serializer &serializer) final;

	bool operator==(const Material &other) const = default;
};
