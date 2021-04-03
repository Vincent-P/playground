#pragma once
#include "base/types.hpp"
#include "render/vulkan/resources.hpp"

struct PACKED Vertex
{
    float3 position;
    float pad00;
    float3 normal;
    float pad01;
    float2 uv0;
    float2 uv1;
    float4 color0 = float4(1.0f);

    bool operator==(const Vertex &other) const = default;
};

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

struct PACKED Material
{
    float4 base_color_factor       = float4(1.0f);
    float4 emissive_factor         = float4(0.0f);
    float metallic_factor          = 0.0f;
    float roughness_factor         = 1.0f;
    u32 base_color_texture         = u32_invalid;
    u32 normal_texture             = u32_invalid;
    u32 metallic_roughness_texture = u32_invalid;
    u32 pad00 = 0;
    u32 pad01 = 0;
    u32 pad10 = 0;

    bool operator==(const Material &other) const = default;
};

struct Mesh
{
    vulkan::PrimitiveTopology topology;

    u32 index_offset = u32_invalid;
    u32 index_count = u32_invalid;

    u32 vertex_offset = u32_invalid;
    u32 vertex_count = u32_invalid;

    bool operator==(const Mesh &other) const = default;
};
