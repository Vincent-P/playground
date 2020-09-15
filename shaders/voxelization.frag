#include "types.h"
#include "globals.h"
#include "voxels.h"
#include "pbr.h"

#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;

layout(set = 1, binding = 1) uniform VO {
    VoxelOptions voxel_options;
};

layout(set = 1, binding = 3, r32ui) uniform uimage3D voxels_albedo;
layout(set = 1, binding = 4, r32ui) uniform uimage3D voxels_normal;

layout (set = 1, binding = 5) uniform CD {
    float4 cascades_depth_slices[4];
};

struct CascadeMatrix
{
    float4x4 view;
    float4x4 proj;
};

layout (set = 1, binding = 6) uniform CM {
    CascadeMatrix cascade_matrices[10];
};

layout(set = 1, binding = 7) uniform sampler2D shadow_cascades[];


const float3 cascade_colors[] = {
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0),
    float3(0.0, 1.0, 1.0),
    };

void main()
{
    /// --- Cascaded shadow
    int cascade_idx = -1;
    float4 shadow_coord = float4(0.0);
    float2 uv = float2(0.0);
    for (cascade_idx = -1; cascade_idx < 4; cascade_idx++)
    {
        cascade_idx += 1;
        CascadeMatrix matrices = cascade_matrices[cascade_idx];
        shadow_coord = (matrices.proj * matrices.view) * vec4(inWorldPos, 1.0);
        shadow_coord /= shadow_coord.w;
        uv = 0.5f * (shadow_coord.xy + 1.0);

        if (0.0 < uv.x && uv.x < 1.0
            && 0.0 < uv.y && uv.y < 1.0
            && 0.0 <= shadow_coord.z  && shadow_coord.z <= 1.0) {
            break;
        }
    }

    float dist = texture(shadow_cascades[nonuniformEXT(cascade_idx)], uv).r;
    const float BIAS = 0.0001;
    float visibility = 1.0;
    if (dist > shadow_coord.z + BIAS) {
        visibility = 0.00;
    }

    vec4 base_color = texture(global_textures[constants.base_color_idx], inUV0);
    if (base_color.a < 0.1) {
        discard;
    }

    vec3 normal = vec3(0.0);
    getNormalM(normal, inWorldPos, inNormal, global_textures[constants.normal_map_idx], inUV0);

    // PBR
    float3 N = normal;
    float3 V = normalize(global.camera_pos - inWorldPos);
    float3 albedo = base_color.rgb;
    float4 metallic_roughness = texture(global_textures[constants.metallic_roughness_idx], inUV0);
    float metallic = metallic_roughness.b;
    float roughness = metallic_roughness.g;

    float3 L = global.sun_direction; // point towards sun
    float3 H = normalize(V + L);
    float3 radiance = global.sun_illuminance; // wrong unit

    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    float3 F0 = float3(0.04);
    F0        = mix(F0, albedo, metallic);
    float3 F  = fresnelSchlick(max(dot(H, V), 0.0), F0);

    float3 kS = F;
    float3 kD = float3(1.0) - kS;
    kD *= 1.0 - metallic; // disable diffuse for metallic materials

    float3 numerator    = NDF * G * F;
    float denominator   = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
    float3 specular     = numerator / max(denominator, 0.001);

    float NdotL = max(dot(N, L), 0.0);
    float3 color = visibility * (kD * albedo / PI + specular) * radiance * NdotL;

    // output:
    ivec3 voxel_pos = WorldToVoxel(inWorldPos, voxel_options);

    imageAtomicAverageRGBA8(voxels_albedo, voxel_pos, color);
    imageAtomicAverageRGBA8(voxels_normal, voxel_pos, EncodeNormal(normal));
}
