#pragma once
#include <exo/types.h>
#include "render/vulkan/resources.h"

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
