#ifndef PBR_H
#define PBR_H

#include "types.h"

layout (set = 0, binding = 1) uniform sampler2D global_textures[];

struct GltfPushConstant
{
    // uniform
    u32 node_idx;

    // textures
    u32 base_color_idx;
    u32 normal_map_idx;
    u32 metallic_roughness_idx;

    // material
    float4 base_color_factor;
    float  metallic_factor;
    float  roughness_factor;
    float2 pad10;
};

layout(push_constant) uniform GC {
    GltfPushConstant constants;
};


// macro because compiler complains about passing normal texture as function argument
#define getNormalM(res, world_pos, vertex_normal, normal_texture, uv)    \
    {                                                                               \
        vec3 tangentNormal = texture(normal_texture, uv).xyz * 2.0 - 1.0;           \
                                                                                    \
        vec3 q1  = dFdx(world_pos);                                                 \
        vec3 q2  = dFdy(world_pos);                                                 \
        vec2 st1 = dFdx(uv);                                                        \
        vec2 st2 = dFdy(uv);                                                        \
                                                                                    \
        vec3 N   = normalize(vertex_normal);                                        \
        vec3 T   = normalize(q1 * st2.t - q2 * st1.t);                              \
        vec3 B   = -normalize(cross(N, T));                                         \
        mat3 TBN = mat3(T, B, N);                                                   \
                                                                                    \
        res = normalize(TBN * tangentNormal);                                       \
    }

#if 0
vec3 getNormal(vec3 world_pos, vec3 vertex_normal, in sampler2D normal_texture, vec2 uv)
{
    // Perturb normal, see http://www.thetenthplanet.de/archives/1180
    vec3 tangentNormal = texture(normal_texture, uv).xyz * 2.0 - 1.0;

    vec3 q1 = dFdx(world_pos);
    vec3 q2 = dFdy(world_pos);
    vec2 st1 = dFdx(uv);
    vec2 st2 = dFdy(uv);

    vec3 N = normalize(vertex_normal);
    vec3 T = normalize(q1 * st2.t - q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}
#endif

float DistributionGGX(vec3 N, vec3 H, float roughness)
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

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

#endif
