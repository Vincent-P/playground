// -*- mode: glsl; -*-

#ifndef PBR_H
#define PBR_H

#extension GL_EXT_nonuniform_qualifier : require

#include "types.h"
#include "constants.h"

struct PushConstant
{
    u32 draw_idx;
    u32 pad00;
    u32 pad01;
    u32 pad02;
};

struct Vertex
{
    float3 position;
    float pad00;
    float3 normal;
    float pad01;
    float2 uv0;
    float2 uv1;
    float4 color0;
    float4 joint0;
    float4 weight0;
};

struct Material
{
    float4 base_color_factor;

    float metallic_factor;
    float roughness_factor;
    u32 base_color_texture;
    u32 normal_texture;

    u32 metallic_roughness_texture;
};

struct Primitive
{
    u32 material;
    u32 first_index;
    u32 first_vertex;
    u32 index_count;

    float3 aab_min;
    u32 rendering_mode;

    float3 aab_max;
    u32 pad00;
};


struct DrawData
{
    u32 transform_idx;
    u32 vertex_idx;
    u32 material_idx;
    u32 primitive_idx;
};

layout (set = 0, binding = 1) uniform sampler2D global_textures[];
layout (set = 0, binding = 1) uniform sampler3D global_textures_3d[];
// layout(push_constant) uniform DrawIndex {
//     PushConstant constants;
// };

/// --- Textures

#ifndef PBR_NO_NORMALS // dFdx/dFdy are only available in fragment shaders
float3 get_normal_from_map(Material material, float3 world_pos, float3 vertex_normal, float2 uv)
{
    // Perturb normal, see http://www.thetenthplanet.de/archives/1180
    float3 tangentNormal = texture(global_textures[material.normal_texture], uv).xyz * 2.0 - 1.0;

    float3 q1 = dFdx(world_pos);
    float3 q2 = dFdy(world_pos);
    float2 st1 = dFdx(uv);
    float2 st2 = dFdy(uv);

    float3 N = normalize(vertex_normal);
    float3 T = normalize(q1 * st2.t - q2 * st1.t);
    float3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

float3 get_normal(Material material, float3 world_pos, float3 vertex_normal, float2 uv)
{
    if (material.normal_texture != u32_invalid)
    {
        return get_normal_from_map(material, world_pos, vertex_normal, uv);
    }
    return vertex_normal;
}
#endif

float4 get_base_color(Material material, float2 uv)
{
    float4 base_color = material.base_color_factor;
    if (material.base_color_texture != u32_invalid)
    {
        base_color *= texture(global_textures[material.base_color_texture], uv);
    }
    return base_color;
}

float2 get_metallic_roughness(Material material, float2 uv)
{
    float2 metallic_roughness = float2(material.metallic_factor, material.roughness_factor);
    if (material.metallic_roughness_texture != u32_invalid)
    {
        metallic_roughness *= texture(global_textures[material.metallic_roughness_texture], uv).bg;
    }
    return metallic_roughness;
}

/// --- PBR equations

float distribution_ggx(float3 N, float3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float geometry_shlick_ggx(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float geometry_smith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = geometry_shlick_ggx(NdotV, roughness);
    float ggx1  = geometry_shlick_ggx(NdotL, roughness);

    return ggx1 * ggx2;
}

float3 fresnel_shlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float safe_dot(float3 a, float3 b)
{
    return max(dot(a, b), 0.0);
}

// albedo: point's color
// N: point's normal
// V: view floattor from point to camera
// metallic:
// roughness:
// L: light direction from light to point
float3 BRDF(float3 albedo, float3 N, float3 V, float metallic, float roughness, float3 L)
{
    float3 H = normalize(L + V);

    float3 F0 = float3(0.04);
    F0        = mix(F0, albedo, metallic);
    // Fresnel Equation
    float3 F  = fresnel_shlick(max(dot(H, V), 0.0), F0);

    float3 kS = F;
    float3 kD = float3(1.0) - kS;
    // metallic materials dont have diffuse reflections
    kD *= 1.0 - metallic;

    float3 lambert_diffuse = albedo / PI;

    // implicitly contains kS
    float3 cookterrance_specular =  distribution_ggx(N, H, roughness) * geometry_smith(N, V, L, roughness) * F
                                 / /* -------------------------------------------------------------------------*/
                                    max(             4.0 * safe_dot(N, V) * safe_dot(N, L)            , 0.001);


    return (kD * lambert_diffuse + cookterrance_specular);
}

#endif
