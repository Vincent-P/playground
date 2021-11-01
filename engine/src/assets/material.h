#pragma once
#include <exo/prelude.h>
#include <exo/handle.h>

#include "assets/asset.h"

struct Texture;

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

struct TextureTransform
{
    float2 offset;   // The offset of the UV coordinate origin as a factor of the texture dimensions.
    float2 scale;    // The scale factor applied to the components of the UV coordinates.
    float  rotation; // Rotate the UVs by this many radians counter-clockwise around the origin. This is equivalent to a similar rotation of the image clockwise.

    bool operator==(const TextureTransform &other) const = default;
};

// Dependencies: Textures
struct Material : Asset
{
    float4           base_color_factor          = float4(1.0f);
    float4           emissive_factor            = float4(0.0f);
    float            metallic_factor            = 1.0f;
    float            roughness_factor           = 1.0f;
    Handle<Texture>  base_color_texture         = {};
    Handle<Texture>  normal_texture             = {};
    Handle<Texture>  metallic_roughness_texture = {};
    TextureTransform uv_transform               = {};

    const char *type_name() final { return "Material"; }
    void from_flatbuffer(const void */*data*/, usize /*len*/) final {}
    void to_flatbuffer(flatbuffers::FlatBufferBuilder &/*builder*/, u32 &/*o_offset*/, u32 &/*o_size*/) const final {}

    bool operator==(const Material &other) const = default;
};
