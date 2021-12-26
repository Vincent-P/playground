#pragma shader_stage(compute)

#include "engine/globals.h"
#include "base/constants.h"
#include "base/hash.h"
#include "base/maths.h"
#include "engine/raytracing.h"
#include "engine/bvh.h"
#include "base/sequences.h"
#include "base/color_map.h"

///
struct DerivativesOutput
{
    float3 db_dx;
    float3 db_dy;
};

// Computes the partial derivatives of a triangle from the projected screen space vertices
DerivativesOutput compute_partial_deritatives(float2 s0, float2 s1, float2 s2)
{
    DerivativesOutput result;
    float d = 1.0 / determinant(float2x2(s2 - s1, s0 - s1));
    result.db_dx = float3(s1.y - s2.y, s2.y - s0.y, s0.y - s1.y) * d;
    result.db_dy = float3(s2.x - s1.x, s0.x - s2.x, s1.x - s0.x) * d;
    return result;
}

// Helper functions to interpolate vertex attributes at point 'd' using the partial derivatives
float3 interpolate_attribute_3(float3x3 attributes, float3 db_dx, float3 db_dy, float2 d)
{
    float3 attribute_x = attributes * db_dx;
    float3 attribute_y = attributes * db_dy;
    float3 attribute_s = float3(attributes[0][0], attributes[1][0], attributes[2][0]); // row 0

    return (attribute_s + d.x * attribute_x + d.y * attribute_y);
}

float interpolate_attribute(float3 attributes, float3 db_dx, float3 db_dy, float2 d)
{
    float attribute_x = dot(attributes, db_dx);
    float attribute_y = dot(attributes, db_dy);
    float attribute_s = attributes[0];

    return (attribute_s + d.x * attribute_x + d.y * attribute_y);
}

struct GradientInterpolationResults
{
    float2 interp;
    float2 dx;
    float2 dy;
};

// Interpolate 2D attributes using the partial derivatives and generates dx and dy for texture sampling.
GradientInterpolationResults interpolate_attribute_with_gradient(float2x3 attributes, float3 db_dx, float3 db_dy, float2 d, float2 p_two_over_res)
{
    float3 attr0 = float3(attributes[0][0], attributes[1][0], attributes[2][0]); // row 0
    float3 attr1 = float3(attributes[0][1], attributes[1][1], attributes[2][1]); // row 1
    float2 attribute_x = float2(dot(db_dx, attr0), dot(db_dx, attr1));
    float2 attribute_y = float2(dot(db_dy, attr0), dot(db_dy, attr1));
    float2 attribute_s = attributes[0]; // col 0

    GradientInterpolationResults result;
    result.dx = attribute_x * p_two_over_res.x;
    result.dy = attribute_y * p_two_over_res.y;
    result.interp = (attribute_s + d.x * attribute_x + d.y * attribute_y);
    return result;
}

struct Deritatives
{
    float3 dx;
    float3 dy;
    float2 d;
    float3 one_over_w;
};

Deritatives compute_deritatives(float2 clip_space, float4 p0, float4 p1, float4 p2)
{
    Deritatives res;

    // transform vertex positions to clip space
    float4 ndc0 = globals.camera_projection * (globals.camera_view * p0);
    float4 ndc1 = globals.camera_projection * (globals.camera_view * p1);
    float4 ndc2 = globals.camera_projection * (globals.camera_view * p2);

    // perspective divide, c012 are now in ndc space
    res.one_over_w = 1.0f / float3(ndc0.w, ndc1.w, ndc2.w);
    ndc0 *= res.one_over_w[0];
    ndc1 *= res.one_over_w[1];
    ndc2 *= res.one_over_w[2];

    // Compute partial derivatives. This is necessary to interpolate triangle attributes per pixel.
    DerivativesOutput deritatives = compute_partial_deritatives(ndc0.xy, ndc1.xy, ndc2.xy);

    res.dx = deritatives.db_dx;
    res.dy = deritatives.db_dy;

    // Calculate delta vector (d) that points from the projected vertex 0 to the current screen point
    res.d = clip_space - ndc0.xy;
    return res;
};
///

void unpack_uvs(Material material, inout float2 uv0, inout float2 uv1, inout float2 uv2)
{
    mat3 translation = mat3(1,0,0, 0,1,0, material.offset.x, material.offset.y, 1);
    mat3 rotation = mat3(
        cos(material.rotation), sin(material.rotation), 0,
       -sin(material.rotation), cos(material.rotation), 0,
                    0,             0, 1
    );
    mat3 scale = mat3(material.scale.x,0,0, 0,material.scale.y,0, 0,0,1);
    mat3 matrix = translation * rotation * scale;

    uv0 = (matrix * float3(uv0, 1)).xy;
    uv1 = (matrix * float3(uv1, 1)).xy;
    uv2 = (matrix * float3(uv2, 1)).xy;
}

layout(set = SHADER_SET, binding = 0) uniform Options {
    uint sampled_visibility_buffer;
    uint sampled_depth_buffer;
    uint sampled_blue_noise;
    uint storage_hdr_buffer;
};

#define V_BUFFER  global_textures_uint[sampled_visibility_buffer]
#define DEPTH_BUFFER  global_textures[sampled_depth_buffer]
#define HDR_BUFFER global_images_2d_rgba32f[storage_hdr_buffer]

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main()
{
    uint local_idx   = gl_LocalInvocationIndex;
    uint3 global_idx = gl_GlobalInvocationID;
    uint3 group_idx  = gl_WorkGroupID;

    int2 pixel_pos = int2(global_idx.xy);
    int2 output_size = imageSize(HDR_BUFFER);
    if (any(greaterThan(pixel_pos, output_size)))
    {
        return;
    }

    float2 screen_uv         = float2(pixel_pos) / float2(output_size);
    float depth       = texelFetch(DEPTH_BUFFER, pixel_pos, LOD0).r;
    float3 clip_space = float3(screen_uv * 2.0 - float2(1.0), depth);

    // TAA jitter
    float2 texel_size = 1.0 / float2(output_size);
    float2 current_jitter = globals.jitter_offset * texel_size;
    clip_space.xy += current_jitter;

    // Early exit if the pixel does not contain a triangle
    if (depth == 0.0)
    {
        imageStore(HDR_BUFFER, pixel_pos, float4(0.1, 0.1, 0.1, 1.0));
        return;
    }

    uint2 visibility = texelFetch(V_BUFFER, pixel_pos, LOD0).rg;
    uint i_submesh_instance = visibility[0];
    uint triangle_id = visibility[1];

    // Fetch geometry
    SubMeshInstance submesh_instance = get_submesh_instance(i_submesh_instance);
    RenderInstance instance          = get_render_instance(submesh_instance.i_instance);
    RenderMesh mesh                  = get_render_mesh(submesh_instance.i_mesh);
    SubMesh submesh                  = get_submesh(mesh.first_submesh, submesh_instance.i_submesh);
    Material material                = get_material(submesh.i_material);

    // triangle indices
    u32 i0 = get_index(mesh.first_index + submesh.first_index + triangle_id * 3 + 0);
    u32 i1 = get_index(mesh.first_index + submesh.first_index + triangle_id * 3 + 1);
    u32 i2 = get_index(mesh.first_index + submesh.first_index + triangle_id * 3 + 2);

    // vertex positions in model space
    float4 p0 = instance.object_to_world * float4(get_position(mesh.first_position, i0).xyz, 1.0);
    float4 p1 = instance.object_to_world * float4(get_position(mesh.first_position, i1).xyz, 1.0);
    float4 p2 = instance.object_to_world * float4(get_position(mesh.first_position, i2).xyz, 1.0);

    // unpack uvs
    float2 uv0 = get_uv(mesh.first_uv, i0);
    float2 uv1 = get_uv(mesh.first_uv, i1);
    float2 uv2 = get_uv(mesh.first_uv, i2);
    unpack_uvs(material, uv0, uv1, uv2);

    Deritatives deritatives = compute_deritatives(clip_space.xy, p0, p1, p2);

    // Interpolate the 1/w (one_over_w) for all three vertices of the triangle
    // using the barycentric coordinates and the delta vector
    float w = 1.0 / interpolate_attribute(deritatives.one_over_w, deritatives.dx, deritatives.dy, deritatives.d);

    // Reconstruct the Z value at this screen point performing only the necessary matrix * vector multiplication
    // operations that involve computing Z
    float z = globals.camera_projection[2][2] * w + globals.camera_projection[3][2];

    // Calculate the world position coordinates:
    // First the projected coordinates at this point are calculated using In.screenPos and the computed Z value at this point.
    // Then, multiplying the perspective projected coordinates by the inverse view-projection matrix (invVP) produces world coordinates
    float3 world_pos = (globals.camera_view_inverse * (globals.camera_projection_inverse * float4(clip_space.xy * w, z, w))).xyz;

    // TEXTURE COORD INTERPOLATION
    // Apply perspective correction to texture coordinates
    float2x3 texCoords;
    texCoords[0] = uv0 * deritatives.one_over_w[0];
    texCoords[1] = uv1 * deritatives.one_over_w[1];
    texCoords[2] = uv2 * deritatives.one_over_w[2];

    // Interpolate texture coordinates and calculate the gradients for texture sampling with mipmapping support
    GradientInterpolationResults results = interpolate_attribute_with_gradient(texCoords, deritatives.dx, deritatives.dy, deritatives.d, 1.0 / output_size);

    const float near_plane = 0.1;
    const float far_plane = 100.0;
    float linearZ = z / (w * near_plane);
    float mip = pow(pow(linearZ, 0.9f) * 5.0f, 1.5f);

    float2 texCoordDX = results.dx * w * mip;
    float2 texCoordDY = results.dy * w * mip;
    float2 texCoord   = results.interp * w;




    // Read material
    float3 albedo = material.base_color_factor.rgb;
    if (material.base_color_texture != u32_invalid)
    {
        albedo *= textureGrad(global_textures[nonuniformEXT(material.base_color_texture)], texCoord, texCoordDX, texCoordDY).rgb;
    }

    // Compute tangent frame
    float3 e1   = p1.xyz - p0.xyz;
    float3 e2   = p2.xyz - p0.xyz;
    float3 surface_normal = normalize(cross(e1, e2));
    float3 tangent;
    float3 bitangent;
    make_orthogonal_coordinate_system(surface_normal, bitangent, tangent);
    mat3 tangent_to_world = mat3(tangent, bitangent, surface_normal);
    mat3 world_to_tangent = transpose(tangent_to_world);

    // Create AO ray
    Ray ray;
    ray.t_min     = 0.1;
    ray.t_max     = 0.6;
    ray.origin    = world_pos + 0.025 * surface_normal;

    // Init hash + noise
    float2 u1_u2;

    uint3 rng_seed = uint3(pixel_pos, globals.frame_count);
    float3 rng = hash_to_float3(hash3(rng_seed));

    float3 noise = float3(0.0);
    if (sampled_blue_noise != u32_invalid)
    {
        // fetch from texture
        noise = texelFetch(global_textures[sampled_blue_noise], pixel_pos % int2(256, 256), LOD0).rgb;
        // animate blue noise
        noise.xy = apply_r2_sequence(noise.xy, globals.frame_count % 64);
        u1_u2 = noise.xy;
    }
    else
    {
        u1_u2 = rng.xy;
    }

    ray.direction = tangent_to_world * sample_cosine_weighted_hemisphere(u1_u2);

    HitInfo hit_info;
    float ao = float(tlas_any_hit(ray, hit_info) == false);
    float3 radiance = ao * albedo;


    #if 0
    imageStore(HDR_BUFFER, pixel_pos, float4(TurboColormap(float(hit_info.box_inter_count) / 100.0), 1.0));
    imageStore(HDR_BUFFER, pixel_pos, float4(material.base_color_factor.rgb, 1.0));

    return;
    #endif

    imageStore(HDR_BUFFER, pixel_pos, float4(radiance, 1.0));
}
