// -*- mode: glsl; -*-

#ifndef BVH_HEADER
#define BVH_HEADER

#include "engine/globals.h"

struct HitInfo
{
    float d;
    u32 box_inter_count;
    float3 barycentrics;
    u32 triangle_id;
    u32 instance_id;
};

bool blas_closest_hit(Ray ray, u32 i_instance, out HitInfo hit_info)
{
    hit_info.d = 1.0 / 0.0;
    hit_info.box_inter_count = 0;
    hit_info.barycentrics = float3(0.0);
    hit_info.triangle_id = u32_invalid;
    hit_info.instance_id = u32_invalid;

    RenderInstance instance = get_render_instance(i_instance);
    RenderMesh mesh = get_render_mesh(instance.i_render_mesh);

    ray.origin    = (instance.world_to_object * float4(ray.origin, 1.0)).xyz;
    ray.direction = (instance.world_to_object * float4(ray.direction, 0.0)).xyz;

    float3 inv_ray_dir = 1.0 / ray.direction;

    uint i_node = 0;
    while (i_node != u32_invalid)
    {
        BVHNode node = get_blas_node(mesh.bvh_root, i_node);

        if (node.prim_index != u32_invalid)
        {
            u32    i_v0 = get_index(mesh.first_index + node.prim_index + 0);
            u32    i_v1 = get_index(mesh.first_index + node.prim_index + 1);
            u32    i_v2 = get_index(mesh.first_index + node.prim_index + 2);
            float3 p0   = get_position(mesh.first_position, i_v0).xyz;
            float3 p1   = get_position(mesh.first_position, i_v1).xyz;
            float3 p2   = get_position(mesh.first_position, i_v2).xyz;

            float d = 0.0;
            Triangle tri;
            tri.v0 = p0;
            tri.e0 = p1 - p0;
            tri.e1 = p2 - p0;
            float3 barycentrics = triangle_intersection(ray, tri, d);

            if (0.0 < d && d < hit_info.d)
            {
                hit_info.d            = d;
                hit_info.barycentrics = barycentrics;
                hit_info.triangle_id  = node.prim_index;
            }
        }
        else
        {
            bool hit_bbox = fast_box_intersection(node.bbox_min, node.bbox_max, ray, inv_ray_dir);
            if (hit_bbox)
            {
                hit_info.box_inter_count += 1;
                i_node += 1;
                continue;
            }
        }

        // the ray missed the triangle or the node's bounding box, skip the subtree
        i_node = node.next_node;
    }

    return hit_info.d < 1.0 / 0.0;
}

bool tlas_closest_hit(Ray ray, out HitInfo hit_info)
{
    float3 inv_ray_dir = 1.0 / ray.direction;

    hit_info.d = 1.0 / 0.0;
    hit_info.box_inter_count = 0;
    hit_info.barycentrics = float3(0.0);
    hit_info.triangle_id = u32_invalid;
    hit_info.instance_id = u32_invalid;

    uint i_node = 0;
    uint iter = 0;
    while (i_node != u32_invalid && iter < 500)
    {
        iter += 1;

        BVHNode node = get_tlas_node(i_node);

        bool hit_bbox = fast_box_intersection(node.bbox_min, node.bbox_max, ray, inv_ray_dir);
        if (hit_bbox)
        {
            hit_info.box_inter_count += 1;

            if (node.prim_index != u32_invalid)
            {
                HitInfo leaf_hit_info;
                // intersect primitive
                bool hit_leaf = blas_closest_hit(ray, node.prim_index, leaf_hit_info);
                hit_info.box_inter_count += leaf_hit_info.box_inter_count;
                if (leaf_hit_info.d < hit_info.d)
                {
                    hit_info.instance_id = node.prim_index;
                    hit_info.d = leaf_hit_info.d;
                    hit_info.barycentrics = leaf_hit_info.barycentrics;
                    hit_info.triangle_id  = leaf_hit_info.triangle_id;
                }
                i_node = node.next_node;
            }
            else
            {
                i_node += 1;
                continue;
            }
        }

        i_node = node.next_node;
    }

    return hit_info.d < 1.0 / 0.0;
}

bool blas_any_hit(Ray ray, u32 i_instance, out HitInfo hit_info)
{
    hit_info.d = 1.0 / 0.0;
    hit_info.box_inter_count = 0;
    hit_info.barycentrics = float3(0.0);
    hit_info.triangle_id = u32_invalid;
    hit_info.instance_id = u32_invalid;

    RenderInstance instance = get_render_instance(i_instance);
    RenderMesh mesh = get_render_mesh(instance.i_render_mesh);

    ray.origin    = (instance.world_to_object * float4(ray.origin, 1.0)).xyz;
    ray.direction = (instance.world_to_object * float4(ray.direction, 0.0)).xyz;

    float3 inv_ray_dir = 1.0 / ray.direction;

    uint i_node = 0;
    while (i_node != u32_invalid)
    {
        BVHNode node = get_blas_node(mesh.bvh_root, i_node);

        if (node.prim_index != u32_invalid)
        {
            u32    i_v0 = get_index(mesh.first_index + node.prim_index + 0);
            u32    i_v1 = get_index(mesh.first_index + node.prim_index + 1);
            u32    i_v2 = get_index(mesh.first_index + node.prim_index + 2);
            float3 p0   = get_position(mesh.first_position, i_v0).xyz;
            float3 p1   = get_position(mesh.first_position, i_v1).xyz;
            float3 p2   = get_position(mesh.first_position, i_v2).xyz;

            float d = 0.0;
            Triangle tri;
            tri.v0 = p0;
            tri.e0 = p1 - p0;
            tri.e1 = p2 - p0;
            float3 barycentrics = triangle_intersection(ray, tri, d);

            if (0.0 < d)
            {
                hit_info.d            = d;
                hit_info.barycentrics = barycentrics;
                hit_info.triangle_id  = node.prim_index;
                return true;
            }
        }
        else
        {
            bool hit_bbox = fast_box_intersection(node.bbox_min, node.bbox_max, ray, inv_ray_dir);
            if (hit_bbox)
            {
                hit_info.box_inter_count += 1;
                i_node += 1;
                continue;
            }
        }

        // the ray missed the triangle or the node's bounding box, skip the subtree
        i_node = node.next_node;
    }

    return false;
}

bool tlas_any_hit(Ray ray, out HitInfo hit_info)
{
    float3 inv_ray_dir = 1.0 / ray.direction;

    hit_info.d = 1.0 / 0.0;
    hit_info.box_inter_count = 0;
    hit_info.barycentrics = float3(0.0);
    hit_info.triangle_id = u32_invalid;
    hit_info.instance_id = u32_invalid;

    uint i_node = 0;
    uint iter = 0;
    while (i_node != u32_invalid && iter < 500)
    {
        iter += 1;

        BVHNode node = get_tlas_node(i_node);

        bool hit_bbox = fast_box_intersection(node.bbox_min, node.bbox_max, ray, inv_ray_dir);
        if (hit_bbox)
        {
            hit_info.box_inter_count += 1;

            if (node.prim_index != u32_invalid)
            {
                HitInfo leaf_hit_info;
                // intersect primitive
                bool hit_leaf = blas_any_hit(ray, node.prim_index, leaf_hit_info);
                hit_info.box_inter_count += leaf_hit_info.box_inter_count;
                if (hit_leaf)
                {
                    hit_info.instance_id = node.prim_index;
                    hit_info.d = leaf_hit_info.d;
                    hit_info.barycentrics = leaf_hit_info.barycentrics;
                    hit_info.triangle_id  = leaf_hit_info.triangle_id;
                    return true;
                }
                i_node = node.next_node;
            }
            else
            {
                i_node += 1;
                continue;
            }
        }

        i_node = node.next_node;
    }

    return false;
}

#endif
