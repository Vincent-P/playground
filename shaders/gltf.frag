#include "constants.h"
#include "globals.h"
#include "pbr.h"
#include "voxels.h"
#include "csm.h"
#include "maths.h"

layout (location = 0) in float3 i_world_pos;
layout (location = 1) in float3 i_normal;
layout (location = 2) in float2 i_uv0;
layout (location = 3) in float2 i_uv1;
layout (location = 4) in float4 i_joint0;
layout (location = 5) in float4 i_weight0;

layout (set = 1, binding = 2) uniform VCTD {
    VCTDebug debug;
};

layout (set = 1, binding = 3) uniform VO {
    VoxelOptions voxel_options;
};

layout(set = 1, binding = 4) uniform sampler3D voxels_radiance;
layout(set = 1, binding = 5) uniform sampler3D voxels_directional_volumes[6];

layout(set = 1, binding = 6) uniform sampler2D shadow_cascades[4];

layout(set = 1, binding = 7) buffer GPUMatrices {
    float4 depth_slices[2];
    CascadeMatrix matrices[4];
} gpu_cascades;

layout (location = 0) out float4 o_color;

float4 anisotropic_sample(float3 position, float mip, float3 direction, float3 weight)
{
    float aniso_level = max(mip - 1.0, 0.0);

    // Important: the refactor `textureLod(direction.x < 0.0 ? 0 : 1, position, aniso_level)` DOES NOT work and produce artifacts!!
    float4 sampled =
		  weight.x * (direction.x < 0.0 ? textureLod(voxels_directional_volumes[0], position, aniso_level) : textureLod(voxels_directional_volumes[1], position, aniso_level))
		+ weight.y * (direction.y < 0.0 ? textureLod(voxels_directional_volumes[2], position, aniso_level) : textureLod(voxels_directional_volumes[3], position, aniso_level))
		+ weight.z * (direction.z < 0.0 ? textureLod(voxels_directional_volumes[4], position, aniso_level) : textureLod(voxels_directional_volumes[5], position, aniso_level))
		;
    if (mip < 1.0)
    {
        float4 sampled0 = texture(voxels_radiance, position);
        sampled = mix(sampled0, sampled, clamp(mip, 0.0, 1.0));
    }

    return sampled;
}

// aperture = tan(teta / 2)
float4 trace_cone(float3 origin, float3 direction, float aperture, float max_dist)
{
    // to perform anisotropic sampling
    float3 weight = direction * direction;
    weight = float3(0.0, 0.0, 1.0);

    float3 p = origin;
    float d = max(debug.start * voxel_options.size, 0.1);

    float4 cone_sampled = float4(0.0);
    float occlusion = 0.0;
    float sampling_factor = debug.sampling_factor < 0.2 ? 0.2 : debug.sampling_factor;

    uint iteration = 0;
    while (cone_sampled.a < 1.0f && d < max_dist && iteration < 5000)
    {
        p = origin + d * direction;

        float diameter = 2.0 * aperture * d;
        float mip_level = log2(diameter / voxel_options.size);

        float3 voxel_pos = WorldToVoxelTex(p, voxel_options);

        float4 sampled = anisotropic_sample(voxel_pos, mip_level, direction, weight);

        occlusion += ((1.0 - occlusion) * sampled.a) / ( 1.0 + debug.occlusion_lambda * diameter);

        cone_sampled = cone_sampled + (1.0 - cone_sampled.a) * sampled;

        d += diameter * sampling_factor;
        iteration += 1;
    }

    return float4(cone_sampled.rgb, 1.0 - occlusion);
}

// The 6 diffuse cones setup is from https://simonstechblog.blogspot.com/2013/01/implementing-voxel-cone-tracing.html
const float3 diffuse_cone_directions[] =
{
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.5, 0.866025),
    float3(0.823639, 0.5, 0.267617),
    float3(0.509037, 0.5, -0.7006629),
    float3(-0.50937, 0.5, -0.7006629),
    float3(-0.823639, 0.5, 0.267617)
};

const float diffuse_cone_weights[] =
{
    PI / 4.0,
    3.0 * PI / 20.0,
    3.0 * PI / 20.0,
    3.0 * PI / 20.0,
    3.0 * PI / 20.0,
    3.0 * PI / 20.0,
};

float4 indirect_lighting(float3 albedo, float3 N, float3 V, float metallic, float roughness)
{
    float3 surface_normal = i_normal;
    float3 cone_origin = i_world_pos + voxel_options.size * surface_normal;
    float4 diffuse = float4(0.0);

    // tan(60 degres / 2), 6 cones of 60 degres each are used for the diffuse lighting
    const float aperture = 0.57735f;

    // Find a tangent and a bitangent
    float3 guide = float3(0.0f, 1.0f, 0.0f);
    if (abs(dot(N,guide)) > 0.99) {
        guide = float3(0.0f, 0.0f, 1.0f);
    }

    float3 right = normalize(guide - dot(surface_normal, guide) * surface_normal);
    float3 up = cross(right, surface_normal);

    float3 cone_direction;

    for (uint i = 0; i < 6; i++)
    {
        cone_direction = surface_normal;
        cone_direction += diffuse_cone_directions[i].x * right + diffuse_cone_directions[i].z * up;
        cone_direction = normalize(cone_direction);

        diffuse += /*float4(BRDF(albedo, N, V, metallic, roughness, cone_direction), 1.0) * */ trace_cone(cone_origin, cone_direction, aperture, debug.trace_dist) * diffuse_cone_weights[i];
    }

    return diffuse;
}

/// --- WIP reflection
    float roughnessToVoxelConeTracingApertureAngle(float roughness)
    {
        roughness = clamp(roughness, 0.0, 1.0);
    #if 1
        return tan(0.0003474660443456835 + (roughness * (1.3331290497744692 - (roughness * 0.5040552688878546)))); // <= used in the 64k
    #elif 1
        return tan(acos(pow(0.244, 1.0 / (clamp(2.0 / max(1e-4, (roughness * roughness)) - 2.0, 4.0, 1024.0 * 16.0) + 1.0))));
    #else
        return clamp(tan((PI * (0.5 * 0.75)) * max(0.0, roughness)), 0.00174533102, 3.14159265359);
    #endif
    }

    float4 trace_reflection(float3 albedo, float3 N, float3 V, float metallic, float roughness)
    {
        float3 position = i_world_pos;

        float aperture = tan(roughness);

        float3 direction = normalize(reflect(-V, N));

        return /* float4(BRDF(albedo, N, V, metallic, roughness, direction), 1.0) * */ trace_cone(position, direction, aperture, debug.trace_dist);
    }
/// --- WIP end

const uint poisson_samples_count = 16;
const float2 poisson_disk[] = {
float2( -0.94201624, -0.39906216 ),
float2( 0.94558609, -0.76890725 ),
float2( -0.094184101, -0.92938870 ),
float2( 0.34495938, 0.29387760 ),
float2( -0.91588581, 0.45771432 ),
float2( -0.81544232, -0.87912464 ),
float2( -0.38277543, 0.27676845 ),
float2( 0.97484398, 0.75648379 ),
float2( 0.44323325, -0.97511554 ),
float2( 0.53742981, -0.47373420 ),
float2( -0.26496911, -0.41893023 ),
float2( 0.79197514, 0.19090188 ),
float2( -0.24188840, 0.99706507 ),
float2( -0.81409955, 0.91437590 ),
float2( 0.19984126, 0.78641367 ),
float2( 0.14383161, -0.14100790 )
};

#define EPSILON 0.01

void main()
{
    float3 normal = get_normal(i_world_pos, i_normal, i_uv0);
    float4 base_color = get_base_color(i_uv0);
    float2 metallic_roughness = get_metallic_roughness(i_uv0);

    // PBR
    float3 albedo = base_color.rgb;
    float3 N = i_normal;
    float3 V = normalize(global.camera_pos - i_world_pos);
    float metallic = metallic_roughness.r;
    float roughness = metallic_roughness.g;

    float3 L = global.sun_direction; // point towards sun
    float3 radiance = global.sun_illuminance; // wrong unit
    float NdotL = max(dot(N, L), 0.0);

    /// --- Cascaded shadow
    int gpu_cascade_idx = 0;
    for (gpu_cascade_idx = 0; gpu_cascade_idx < 3; gpu_cascade_idx++)
    {
        uint gpu_cascade_next = gpu_cascade_idx + 1;
        if (gl_FragCoord.z > gpu_cascades.depth_slices[gpu_cascade_next/4][gpu_cascade_next%4]) {
            break;
        }
    }


    CascadeMatrix matrices = gpu_cascades.matrices[gpu_cascade_idx];
    float4 shadow_coord = (matrices.proj * matrices.view) * float4(i_world_pos, 1.0);
    shadow_coord /= shadow_coord.w;
    float2 uv = 0.5 * (shadow_coord.xy + 1.0);

    const float bias = 0.1 * max(0.05f * (1.0f - NdotL), 0.005f);

#define POISSON_DISK 1
#define PCF 1

#if POISSON_DISK
    float3 random_angle_uv = (i_world_pos.xyz*1000)/32;
    float2 random_cos_sin = texture(global_textures_3d[constants.random_rotations_idx], random_angle_uv).xy;

    float shadow = 0.0;
    for (uint i_tap = 0; i_tap < 16; i_tap++)
    {
        uint i_disk = i_tap;
        float2 offset = float2(
            random_cos_sin[0] * poisson_disk[i_disk].x - random_cos_sin[1] * poisson_disk[i_disk].y,
            random_cos_sin[1] * poisson_disk[i_disk].x + random_cos_sin[0] * poisson_disk[i_disk].y
            );

        const float SIZE = 0.001;

#if 1
        float shadow_map_depth = 0.0;
        if (gpu_cascade_idx == 0)
        {
            shadow_map_depth = texture(shadow_cascades[0], uv + SIZE * offset).r;
        }
        else if (gpu_cascade_idx == 1)
        {
            shadow_map_depth = texture(shadow_cascades[1], uv + SIZE * offset).r;
        }
        else if (gpu_cascade_idx == 2)
        {
            shadow_map_depth = texture(shadow_cascades[2], uv + SIZE * offset).r;
        }
        else if (gpu_cascade_idx == 3)
        {
            shadow_map_depth = texture(shadow_cascades[3], uv + SIZE * offset).r;
        }
#else
        // this doesnt work on windows amd, vkCreateGraphicsPipelines fails...
        float shadow_map_depth = texture(shadow_cascades[nonuniformEXT(gpu_cascade_idx)], uv + SIZE * offset).r;
#endif

        if (shadow_map_depth > shadow_coord.z + bias) {
            shadow += 1.0;
        }
    }

    float visibility = 1.0 - (shadow / (poisson_samples_count));

#elif PCF

    float4 texels = textureGather(shadow_cascades[nonuniformEXT(gpu_cascade_idx)], uv);
    float shadow = 0.0;
    for (uint i_shadow = 0; i_shadow < 4; i_shadow++)
    {
        shadow += float(texels[i_shadow] > shadow_coord.z + bias);
    }
    float visibility = 1.0 - shadow / 4;
#else
    float visibility = 1.0 - float(texture(shadow_cascades[nonuniformEXT(gpu_cascade_idx)], uv).r > shadow_coord.z + bias);
#endif

    /// --- Lighting

    float3 direct = visibility * BRDF(albedo, N, V, metallic, roughness, L) * radiance * NdotL;

    float4 indirect =  indirect_lighting(albedo, N, V, metallic, roughness);

    float4 reflection = float4(0); // trace_reflection(albedo, i_normal, V, metallic, roughness);

    // base color
    if (debug.display_selected == 1)
    {
        indirect.rgb = float3(0.0);
        indirect.a = 1.0;
        direct = albedo;
    }
    // normal
    else if (debug.display_selected == 2)
    {
        indirect.rgb = float3(0.0);
        indirect.a = 1.0;
        direct = EncodeNormal(normal);
    }
    // ao
    else if (debug.display_selected == 3)
    {
        direct = float3(0.0);
        indirect.rgb = float3(1.0);
    }
    // indirect
    else if (debug.display_selected == 4)
    {
        direct = float3(0.0);
    }
    // direct
    else if (debug.display_selected == 5)
    {
        indirect.rgb = float3(0.0);
        indirect.a = 1.0;
    }
    // no debug
    else
    {
        indirect.rgb *= albedo / PI;
    }

    float3 composite = direct + indirect.rgb * indirect.a;

    // composite.rgb *= cascade_colors[gpu_cascade_idx];

    o_color = float4(composite, 1.0);
}
