#include "constants.h"
#include "globals.h"
#include "pbr.h"
#include "voxels.h"

#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;
layout (location = 4) in vec4 inJoint0;
layout (location = 5) in vec4 inWeight0;
layout (location = 6) in vec4 inViewPos;

layout (set = 1, binding = 1) uniform VCTD {
    VCTDebug debug;
};

layout (set = 1, binding = 2) uniform VO {
    VoxelOptions voxel_options;
};

layout(set = 1, binding = 3) uniform sampler3D voxels_radiance;
layout(set = 1, binding = 4) uniform sampler3D voxels_directional_volumes[6];

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
    float mip_level = 0.0;

    while (cone_sampled.a < 1.0f && d < max_dist)
    {
        p = origin + d * direction;

        float diameter = 2.0 * aperture * d;
        mip_level = log2(diameter / voxel_options.size);

        vec3 voxel_pos = WorldToVoxelTex(p, voxel_options);

        vec4 sampled = anisotropic_sample(voxel_pos, mip_level, direction, weight);

        cone_sampled += (1.0 - cone_sampled) * sampled;
        occlusion += ((1.0 - occlusion) * sampled.a) / ( 1.0 + debug.occlusion_lambda * diameter);

        d += diameter * sampling_factor;
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
    vec3 cone_origin = inWorldPos;
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

    vec3 direction;

    for (uint i = 0; i < 6; i++)
    {
        direction = N;
        direction += diffuse_cone_directions[i].x * right + diffuse_cone_directions[i].z * up;
        direction = normalize(direction);

        diffuse += float4(BRDF(albedo, N, V, metallic, roughness, direction), 1.0) * trace_cone(cone_origin, direction, aperture, debug.trace_dist) * diffuse_cone_weights[i];
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

        float aperture = roughnessToVoxelConeTracingApertureAngle(0.01);

        float3 cone_direction = normalize(float3(-V.x, 1.0, -V.z));

        // Find a tangent and a bitangent
        vec3 guide = vec3(0.0f, 1.0f, 0.0f);
        if (abs(dot(N,guide)) == 1.0f) {
            guide = vec3(0.0f, 0.0f, 1.0f);
        }
        vec3 right = normalize(guide - dot(N, guide) * N);
        vec3 up = cross(right, N);
        vec3 direction;

        direction = N;
        direction += cone_direction.x * right + cone_direction.z * up;
        direction = normalize(direction);

        return /* float4(BRDF(albedo, N, V, metallic, roughness, direction), 1.0) * */ trace_cone(position, direction, aperture, debug.trace_dist);
    }
/// --- WIP end

const float3 cascade_colors[] = {
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0),
    float3(0.0, 1.0, 1.0),
    };

void main()
{
    /// --- Cascaded shadow

    uint cascade_idx = 0;
    for (uint i = 0; i < 4 - 1; i++) {
        float slice = 1.0 - cascades_depth_slices[i/4][i%4];
        if(gl_FragCoord.z < slice) {
            cascade_idx = i + 1;
        }
    }

    CascadeMatrix matrices = cascade_matrices[cascade_idx];
    vec4 shadow_coord = (matrices.proj * matrices.view) * vec4(inWorldPos, 1.0);
    shadow_coord /= shadow_coord.w;

    float visibility = 1.0;
#if 0
    float2 uv = 0.5f * (shadow_coord.xy + 1.0);

    float dist = texture(shadow_cascades[nonuniformEXT(cascade_idx)], uv).r;

    const float bias = 0.0001;
    if (dist > shadow_coord.z + bias) {
        visibility = global.ambient;
    }
#else
    //
    ivec2 dim = textureSize(shadow_cascades[nonuniformEXT(cascade_idx)], 0).xy;
    float scale = 0.75;
    float dx = scale * 1.0 / float(dim.x);
    float dy = scale * 1.0 / float(dim.y);

    float shadow_factor = 0.0;
    int count = 0;
    const int range = 1;

    for (int x = -range; x <= range; x++) {
        for (int y = -range; y <= range; y++) {
            float2 uv = 0.5f * ((shadow_coord.xy + float2(dx*x, dy*y)) + 1.0);

            float dist = texture(shadow_cascades[nonuniformEXT(cascade_idx)], uv).r;

            const float bias = 0.0001;
            float shadow = 1.0;
            if (dist > shadow_coord.z + bias) {
                shadow = global.ambient;
            }
            shadow_factor += shadow;
            count++;
        }
    }

    visibility = shadow_factor / count;
#endif

    /// --- Lighting
    float3 normal = float3(0.0);
    getNormalM(normal, inWorldPos, inNormal, global_textures[constants.normal_map_idx], inUV0);
    float4 base_color = texture(global_textures[constants.base_color_idx], inUV0);
    float4 metallic_roughness = texture(global_textures[constants.metallic_roughness_idx], inUV0);

    // PBR
    float3 albedo = base_color.rgb;
    float3 N = normal;
    float3 V = normalize(global.camera_pos - inWorldPos);
    float metallic = metallic_roughness.b;
    float roughness = metallic_roughness.g;

    float3 L = global.sun_direction; // point towards sun
    float3 radiance = global.sun_illuminance; // wrong unit
    float NdotL = max(dot(N, L), 0.0);

    float3 direct = visibility * BRDF(albedo, N, V, metallic, roughness, L) * radiance * NdotL;

    float4 indirect = indirect_lighting(albedo, N, V, metallic, roughness);

    // float4 reflection = trace_reflection(albedo, N, V, metallic, roughness);

    // base color
    if (debug.gltf_debug_selected == 1)
    {
        // indirect = reflection;
        // indirect.a = 1.0;
        direct = float3(0.0);
    }
    // normal
    else if (debug.gltf_debug_selected == 2)
    {
        indirect.rgb = float3(0.0);
        indirect.a = 1.0;
        direct = EncodeNormal(normal);
    }
    // ao
    else if (debug.gltf_debug_selected == 3)
    {
        direct = float3(0.0);
        indirect.rgb = float3(1.0);
    }
    // indirect
    else if (debug.gltf_debug_selected == 4)
    {
        direct = float3(0.0);
    }
    // direct
    else if (debug.gltf_debug_selected == 5)
    {
        indirect.rgb = float3(0.0);
        indirect.a = 1.0;
    }
    // no debug
    else
    {
        // indirect += reflection;
    }

    float3 composite = (direct + indirect.rgb) * indirect.a;

    outColor = vec4(composite, 1.0);
}
