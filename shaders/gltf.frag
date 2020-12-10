#include "constants.h"
#include "globals.h"
#include "pbr.h"
#include "voxels.h"
#include "csm.h"
#include "maths.h"

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;
layout (location = 4) in vec4 inJoint0;
layout (location = 5) in vec4 inWeight0;

layout (set = 1, binding = 2) uniform VCTD {
    VCTDebug debug;
};

layout (set = 1, binding = 3) uniform VO {
    VoxelOptions voxel_options;
};

layout(set = 1, binding = 4) uniform sampler3D voxels_radiance;
layout(set = 1, binding = 5) uniform sampler3D voxels_directional_volumes[6];

layout (set = 1, binding = 6) uniform CD {
    float4 cascades_depth_slices[4];
};

layout (set = 1, binding = 7) uniform CM {
    CascadeMatrix cascade_matrices[10];
};

layout(set = 1, binding = 8) uniform sampler2D shadow_cascades[4];

layout (location = 0) out vec4 outColor;

vec4 anisotropic_sample(vec3 position, float mip, float3 direction, vec3 weight)
{
    float aniso_level = max(mip - 1.0, 0.0);

    // Important: the refactor `textureLod(direction.x < 0.0 ? 0 : 1, position, aniso_level)` DOES NOT work and produce artifacts!!
    vec4 sampled =
		  weight.x * (direction.x < 0.0 ? textureLod(voxels_directional_volumes[0], position, aniso_level) : textureLod(voxels_directional_volumes[1], position, aniso_level))
		+ weight.y * (direction.y < 0.0 ? textureLod(voxels_directional_volumes[2], position, aniso_level) : textureLod(voxels_directional_volumes[3], position, aniso_level))
		+ weight.z * (direction.z < 0.0 ? textureLod(voxels_directional_volumes[4], position, aniso_level) : textureLod(voxels_directional_volumes[5], position, aniso_level))
		;
    if (mip < 1.0)
    {
        vec4 sampled0 = texture(voxels_radiance, position);
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

    vec3 p = origin;
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

        vec3 voxel_pos = WorldToVoxelTex(p, voxel_options);

        vec4 sampled = anisotropic_sample(voxel_pos, mip_level, direction, weight);

        occlusion += ((1.0 - occlusion) * sampled.a)/* / ( 1.0 + debug.occlusion_lambda * diameter)*/;
        // cone_sampled = occlusion * cone_sampled + (1.0 - occlusion) *sampled;
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
    vec3 cone_origin = inWorldPos + voxel_options.size * N;
    vec4 diffuse = vec4(0.0);

    // tan(60 degres / 2), 6 cones of 60 degres each are used for the diffuse lighting
    const float aperture = 0.57735f;

    // Find a tangent and a bitangent
    vec3 guide = vec3(0.0f, 1.0f, 0.0f);
    if (abs(dot(N,guide)) == 1.0f) {
        guide = vec3(0.0f, 0.0f, 1.0f);
    }
    vec3 right = normalize(guide - dot(N, guide) * N);
    vec3 up = cross(right, N);

    vec3 cone_direction;

    for (uint i = 0; i < 6; i++)
    {
        cone_direction = N;
        cone_direction += diffuse_cone_directions[i].x * right + diffuse_cone_directions[i].z * up;
        cone_direction = normalize(cone_direction);

        diffuse += float4(BRDF(albedo, N, V, metallic, roughness, cone_direction), 1.0) *  trace_cone(cone_origin, cone_direction, aperture, debug.trace_dist) * diffuse_cone_weights[i];
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
        vec3 position = inWorldPos;

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
    float3 normal = get_normal(inWorldPos, inNormal, inUV0);
    float4 base_color = get_base_color(inUV0);
    float2 metallic_roughness = get_metallic_roughness(inUV0);

    // PBR
    float3 albedo = base_color.rgb;
    float3 N = normal;
    float3 V = normalize(global.camera_pos - inWorldPos);
    float metallic = metallic_roughness.r;
    float roughness = metallic_roughness.g;

    float3 L = global.sun_direction; // point towards sun
    float3 radiance = global.sun_illuminance; // wrong unit
    float NdotL = max(dot(N, L), 0.0);

    /// --- Cascaded shadow
    int cascade_idx = 0;
    for (cascade_idx = 0; cascade_idx < 2; cascade_idx++)
    {
        if (gl_FragCoord.z > cascades_depth_slices[0][cascade_idx]) {
            break;
        }
    }

    CascadeMatrix matrices = cascade_matrices[cascade_idx];
    float4 shadow_coord = (matrices.proj * matrices.view) * float4(inWorldPos, 1.0);
    shadow_coord /= shadow_coord.w;
    float2 uv = 0.5 * (shadow_coord.xy + 1.0);

    const float bias = 0.05 * max(0.05f * (1.0f - NdotL), 0.005f);

    float3 random_angle_uv = (inWorldPos.xyz*1000)/32;
    float2 random_cos_sin = texture(global_textures_3d[constants.random_rotations_idx], random_angle_uv).xy;

    float shadow = 0.0;
    for (uint i_tap = 0; i_tap < poisson_samples_count; i_tap++)
    {
        float2 offset = float2(
            random_cos_sin[0] * poisson_disk[i_tap].x - random_cos_sin[1] * poisson_disk[i_tap].y,
            random_cos_sin[1] * poisson_disk[i_tap].x + random_cos_sin[0] * poisson_disk[i_tap].y
            );

        const float SIZE = 0.001;

        float shadow_map_depth = texture(shadow_cascades[nonuniformEXT(cascade_idx)], uv + SIZE * offset).r;
        if (shadow_map_depth > shadow_coord.z + bias) {
            shadow += 1.0;
        }
    }

    float visibility = 1.01 - (shadow / (poisson_samples_count));

    /// --- Lighting

    float3 direct = visibility * BRDF(albedo, N, V, metallic, roughness, L) * radiance * NdotL;

    float4 indirect =  indirect_lighting(albedo, N, V, metallic, roughness);

    float4 reflection = float4(0); // trace_reflection(albedo, inNormal, V, metallic, roughness);

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
    }

    float3 composite = (direct + indirect.rgb) * indirect.a;

    // composite.rgb *= cascade_colors[cascade_idx];

    outColor = vec4(composite, 1.0);
}
