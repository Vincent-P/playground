#pragma shader_stage(compute)
#include "types.h"
#include "constants.h"
#include "globals.h"
#include "raytracing.h"

layout(set = 1, binding = 0) uniform Options {
    uint storage_output_frame;
};

layout (set = 1, binding = 1, rgba16f) uniform image2D output_frame;

struct Material
{
    float3 albedo;
    float3 emissive;
};

struct Shape
{
    Sphere sphere;
    Box box;
    Material material;
};

const Shape scene[] =
{
    {{float3(0.0), 0.0},            {float3(0.0), float3(10, 1, 10), float3(0.1, 1, 0.1)}, {float3(0.0, 1.0, 0.0), float3(0.0)}},
    {{float3(0.0, 3.0,  0.0), 2.0}, {float3(0.0), float3(0.0), float3(0.0)},               {float3(1.0, 1.0, 0.0), float3(0.0)}},
    {{float3(5.0, 15.0,  0.0), 5}, {float3(0.0), float3(0.0), float3(0.0)},               {float3(0.0, 0.0, 0.0), float3(100.0)}},
};

bool intersect_scene(Ray ray, float3 inv_ray_dir, out float d, out float3 normal, out uint closest_shape)
{
    closest_shape = ~0u;
    d             = 999999999999.999999999999;
    normal        = float3(0.0, 1.0, 0.0);

    for (uint i_shape = 0; i_shape < 3; i_shape += 1)
    {
        float  hit_dist = 0.0;
        float3 hit_normal = float3(0.0);

        bool hit = false;
        // it is a sphere
        if (scene[i_shape].sphere.radius > 0.0)
        {
            hit = ray_sphere_nearest_intersection(ray, scene[i_shape].sphere, hit_dist, hit_normal);
        }
        // guess it is a box after all
        else
        {
            hit = ourIntersectBoxCommon(scene[i_shape].box, ray, hit_dist, hit_normal, inv_ray_dir);
        }

        if (hit && hit_dist < d)
        {
            closest_shape = i_shape;
            d             = hit_dist;
            normal        = hit_normal;
        }
    }

    return closest_shape != ~0u;
}

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main()
{
    uint  local_idx  = gl_LocalInvocationIndex;
    uint3 global_idx = gl_GlobalInvocationID;
    uint3 group_idx  = gl_WorkGroupID;

    int2 pixel_pos = int2(global_idx.xy);
    int2 output_size = imageSize(output_frame);

    if (any(greaterThan(pixel_pos, output_size)))
    {
        return;
    }
    // initialize a random number state based on frag coord and frame
    uint seed = init_seed(pixel_pos, globals.frame_count);

    float2 uv = float2(pixel_pos) / globals.resolution;
    float3 clip_space = float3(uv * 2.0 - 1.0, 0.0001);
    float4 h_pos      = globals.camera_view_inverse * globals.camera_projection_inverse * float4(clip_space, 1.0);
    h_pos /= h_pos.w;

    Ray ray;
    ray.origin    = globals.camera_position.xyz;
    ray.direction = normalize(h_pos.xyz - ray.origin);
    float3 inv_ray_dir = 1.0 / ray.direction;

    float3 o_color = float3(0.0);
    float3 troughput = float3(1.0);

    float d = 0.0;
    float3 N = float3(0.0);
    uint i_shape = 0;

    for (uint i_bounce = 0; i_bounce < 4; i_bounce += 1)
    {
        bool hit = intersect_scene(ray, inv_ray_dir, d, N, i_shape);
        if (!hit) break;

        const float ray_hit_bias = 0.1;
        ray.origin = ray.origin + d * ray.direction + N * ray_hit_bias;
        ray.direction = normalize(N + random_unit_vector(seed));
        inv_ray_dir = 1.0 / ray.direction;

        o_color += scene[i_shape].material.emissive * troughput;
        troughput *= scene[i_shape].material.albedo;

    }

    imageStore(output_frame, pixel_pos, vec4(o_color, 1.0));
}
