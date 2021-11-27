#pragma once
#include <exo/prelude.h>
#include <exo/collections/handle.h>
#include <exo/cross/uuid.h>

#include "assets/asset.h"
#include "schemas/texture_header_generated.h"

struct Texture;

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
    cross::UUID      base_color_texture         = {};
    cross::UUID      normal_texture             = {};
    cross::UUID      metallic_roughness_texture = {};
    TextureTransform uv_transform               = {};

    const char *type_name() const final
    {
        return "Material";
    }
    void from_flatbuffer(const void *data, usize len) final;
    void to_flatbuffer(flatbuffers::FlatBufferBuilder &builder, u32 &o_offset, u32 &o_size) const final;

    void display_ui() final;

    bool operator==(const Material &other) const = default;
};
