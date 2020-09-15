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


const vec3 diffuseConeDirections[] =
{
    vec3(0.0f, 1.0f, 0.0f),
    vec3(0.0f, 0.5f, 0.866025f),
    vec3(0.823639f, 0.5f, 0.267617f),
    vec3(0.509037f, 0.5f, -0.7006629f),
    vec3(-0.50937f, 0.5f, -0.7006629f),
    vec3(-0.823639f, 0.5f, 0.267617f)
};

const float diffuseConeWeights[] =
{
    PI / 4.0f,
    3.0f * PI / 20.0f,
    3.0f * PI / 20.0f,
    3.0f * PI / 20.0f,
    3.0f * PI / 20.0f,
    3.0f * PI / 20.0f,
};

vec4 AnisotropicSample(vec3 position, float mip, uvec3 faces, vec3 weight)
{
    float aniso_level = max(mip - 1.0, 0.0);
    vec4 sampled = weight.x * textureLod(voxels_directional_volumes[faces.x], position, aniso_level)
                 + weight.y * textureLod(voxels_directional_volumes[faces.y], position, aniso_level)
                 + weight.z * textureLod(voxels_directional_volumes[faces.z], position, aniso_level);

    if (mip < 1.0)
    {
        vec4 sampled0 = texture(voxels_radiance, position);
        sampled = mix(sampled0, sampled, clamp(mip, 0.0, 1.0));
    }


    return sampled;
}

// aperture = tan(teta / 2)
vec4 TraceCone(vec3 origin, vec3 direction, float aperture, float max_dist)
{
    // to perform anisotropic sampling
    uvec3 faces;
    faces.x = (direction.x < 0.0) ? 0 : 1;
    faces.y = (direction.y < 0.0) ? 2 : 3;
    faces.z = (direction.z < 0.0) ? 4 : 5;
    vec3 weight = direction * direction;

    vec4 cone_sampled = vec4(0.0);
    vec3 p = origin;
    float d = debug.start * voxel_options.size;

    float occlusion = 0.0;
    float sampling_factor = debug.sampling_factor < 0.2 ? 0.2 : debug.sampling_factor;

    while (cone_sampled.a < 1.0f && d < max_dist)
    {
        p = origin + d * direction;

        float diameter = 2.0 * aperture * d;
        float mip_level = log2(diameter / voxel_options.size);
        vec3 voxel_pos = WorldToVoxelTex(p, voxel_options);

        vec4 sampled = AnisotropicSample(voxel_pos, mip_level, faces, weight);

        cone_sampled += (1.0 - cone_sampled) * sampled;
        occlusion += ((1.0 - occlusion) * sampled.a) / ( 1.0 + debug.occlusion_lambda * diameter);

        d += diameter * sampling_factor;
    }

    return vec4(cone_sampled.rgb , 1.0 - occlusion);
}

vec4 Indirect(vec3 normal)
{
    vec3 position = inWorldPos;
    vec4 diffuse = vec4(0.0);

    const float aperture = 0.57735f;

    // diffuse cone setup
    vec3 guide = vec3(0.0f, 1.0f, 0.0f);

    if (abs(dot(normal,guide)) == 1.0f)
    {
        guide = vec3(0.0f, 0.0f, 1.0f);
    }

    // Find a tangent and a bitangent
    vec3 right = normalize(guide - dot(normal, guide) * normal);
    vec3 up = cross(right, normal);
    vec3 direction;

    for (uint i = 0; i < 6; i++)
    {
        direction = normal;
        direction += diffuseConeDirections[i].x * right + diffuseConeDirections[i].z * up;
        direction = normalize(direction);

        diffuse += TraceCone(position, direction, aperture, debug.trace_dist) * diffuseConeWeights[i];
    }

    return diffuse;
}

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
        visibility = 0.00;
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
                shadow = 0.00;
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
    float3 direct = visibility * (kD * albedo / PI + specular) * radiance * NdotL;

    float4 indirect = base_color * Indirect(normal);
    indirect.a = min(indirect.a ,1.0);

    // base color
    if (debug.gltf_debug_selected == 1)
    {
        indirect.rgb = float3(0.0);
        indirect.a = 1.0;
        direct = base_color.rgb;
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
    }

    float3 composite = (direct + indirect.rgb) * indirect.a;

    outColor = vec4(composite, 1.0);
}
