// -*- mode: glsl; -*-

#ifndef PBR_H
#define PBR_H

#extension GL_EXT_nonuniform_qualifier : require

#include "types.h"
#include "constants.h"

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
// V: scattered light (view vector from point to camera)
// metallic:
// roughness:
// L: incoming light (light direction from light to point)
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
    float cookterrance_specular =  distribution_ggx(N, H, roughness) * geometry_smith(N, V, L, roughness)
                                 / /* -------------------------------------------------------------------------*/
                                    max(             4.0 * safe_dot(N, V) * safe_dot(N, L)            , 0.001);


    return (kD * lambert_diffuse + kS * cookterrance_specular);
}

#endif
