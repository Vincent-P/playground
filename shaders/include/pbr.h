#ifndef PBR_H
#define PBR_H

#include "types.h"

layout (set = 0, binding = 1) uniform sampler2D global_textures[];

struct GltfPushConstant
{
    // uniform
    u32 transform_idx;

    // textures
    u32 base_color_idx;
    u32 normal_map_idx;
    float pad00;

    // material
    float4 base_color_factor;
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
