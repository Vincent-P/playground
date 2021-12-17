#pragma once
#include <exo/collections/handle.h>
#include <exo/os/uuid.h>

#include "assets/asset.h"
namespace exo {struct Serializer;}

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
    float4           base_color_factor          = float4(1.0f);
    float4           emissive_factor            = float4(0.0f);
    float            metallic_factor            = 1.0f;
    float            roughness_factor           = 1.0f;
    exo::UUID      base_color_texture         = {};
    exo::UUID      normal_texture             = {};
    exo::UUID      metallic_roughness_texture = {};
    TextureTransform uv_transform               = {};

    static Asset *create();
    const char *type_name() const final { return "Material"; }
    void serialize(exo::Serializer& serializer) final;

    void display_ui() final;

    bool operator==(const Material &other) const = default;
};
