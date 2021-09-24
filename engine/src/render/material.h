#pragma once
#include <exo/prelude.h>

// unity-like pbr materials?
enum struct MaterialType
{
    Default,
    SubsurfaceScattering,
    Anisotropy,
    SpecularColor,
    Iridescence,
    Translucent
};

PACKED(struct Material
{
    float4 base_color_factor          = float4(1.0f);
    float4 emissive_factor            = float4(0.0f);
    float  metallic_factor            = 1.0f;
    float  roughness_factor           = 1.0f;
    u32    base_color_texture         = u32_invalid;
    u32    normal_texture             = u32_invalid;
    u32    metallic_roughness_texture = u32_invalid;
    float  rotation                   = 0;
    float2 offset                     = float2(0.0f, 0.0f);
    float2 scale                      = float2(1.0f, 1.0f);
    float2 pad00                      = {0.0f};

    bool operator==(const Material &other) const = default;
};)

PACKED(struct TextureTransform
{
    float2 offset;  // The offset of the UV coordinate origin as a factor of the texture dimensions.
    float2 scale;   // The scale factor applied to the components of the UV coordinates.
    float rotation; // Rotate the UVs by this many radians counter-clockwise around the origin. This is equivalent to a similar rotation of the image clockwise.

    bool operator==(const TextureTransform &other) const = default;
};)
