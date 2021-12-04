#pragma shader_stage(compute)

#include "globals.h"
#include "hash.h"
#include "raytracing.h"
#include "pbr.h"
#include "bvh.h"
#include "color_map.h"

layout(set = SHADER_SET, binding = 0) uniform Options {
    u32 storage_output;
};


layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main()
{
    uint  local_idx  = gl_LocalInvocationIndex;
    uint3 global_idx = gl_GlobalInvocationID;
    uint3 group_idx  = gl_WorkGroupID;

    int2 pixel_pos = int2(global_idx.xy);
    int2 output_size = imageSize(global_images_2d_rgba32f[storage_output]);

    if (any(greaterThan(pixel_pos, output_size)))
    {
        return;
    }

    float2 screen_uv  = (float2(pixel_pos) + float2(0.5)) / float2(output_size);
    float3 clip_space = float3(screen_uv * 2.0 - 1.0, 1.0);

    // TAA jitter
    float2 texel_size = 1.0 / float2(output_size);
    float2 current_jitter = globals.jitter_offset * texel_size;
    clip_space.xy += current_jitter;

    // Create a pixel ray
    float4x4 view_rows = transpose(globals.camera_view);
    float aspect_ratio = globals.camera_projection[1][1] / globals.camera_projection[0][0];
    float tan_half_fov_y = 1.0 / globals.camera_projection[1][1];
    Ray ray;
    ray.t_min = 0.0;
    ray.t_max = 1.0 / 0.0;
    ray.origin    = globals.camera_view_inverse[3].xyz;
    ray.direction =
        + (clip_space.x * tan_half_fov_y * aspect_ratio) * view_rows[0].xyz
        + (clip_space.y * tan_half_fov_y) * view_rows[1].xyz
        - view_rows[2].xyz;

    uint3 rng_seed = uint3(pixel_pos, globals.frame_count);

    const uint MAX_BOUNCE = 0;
    const float3 BACKGROUND_COLOR = float3(0.0);
    uint hit_count = 0;
    HitInfo hit_info;
    float3 radiance = float3(0.0);
    float3 throughput = float3(1.0);

    bool hit = tlas_closest_hit(ray, hit_info);
    radiance = TurboColormap(float(hit_info.box_inter_count) / 500.0);

    for (u32 i_bounce = 0; i_bounce < MAX_BOUNCE; i_bounce += 1)
    {
        if (tlas_closest_hit(ray, hit_info) == false)
        {
            radiance = BACKGROUND_COLOR * throughput;
            break;
        }


        RenderInstance instance = get_render_instance(hit_info.instance_id);
        RenderMesh mesh = get_render_mesh(instance.i_render_mesh);


        // -- Fetch geometry information
        u32    i_v0 = get_index(mesh.first_index + hit_info.triangle_id + 0);
        u32    i_v1 = get_index(mesh.first_index + hit_info.triangle_id + 1);
        u32    i_v2 = get_index(mesh.first_index + hit_info.triangle_id + 2);
        float3 p0   = get_position(mesh.first_position, i_v0).xyz;
        float3 p1   = get_position(mesh.first_position, i_v1).xyz;
        float3 p2   = get_position(mesh.first_position, i_v2).xyz;
        float3 e1 = p1 - p0;
        float3 e2 = p2 - p0;

        float3 surface_normal = normalize(cross(e1, e2));

        float3 tangent;
        float3 bitangent;
        make_orthogonal_coordinate_system(surface_normal, bitangent, tangent);
        mat3 tangent_to_world = mat3(tangent, bitangent, surface_normal);
        mat3 world_to_tangent = transpose(tangent_to_world);

        // -- Material
        const float3 albedo = float3(1.0);
        const float3 emissive = float3(0.0);

        // -- BRDF
        float2 rng = hash3(rng_seed).xy;

        float3 wo = world_to_tangent * -ray.direction;
        float3 wi = lambert_sample(rng);
        float3 brdf = lambert_brdf(wo, wi, albedo);
        float pdf = lambert_pdf(wo, wi) ;

        radiance   += emissive * throughput;
        throughput *= brdf / pdf;

        // -- Bounce
        ray.origin    = (ray.origin + hit_info.d * ray.direction) + surface_normal * 0.001;
        ray.direction = tangent_to_world * wi;
        ray.t_min = 0.0;
        ray.t_max = 5.0;

        #if 0
        radiance = fract(ray.origin.xyz);
        break;
        #endif

        break;
    }

    imageStore(global_images_2d_rgba32f[storage_output], pixel_pos, float4(radiance, 1.0));
}
