#pragma shader_stage(compute)

#include "base/types.h"
#include "base/constants.h"
#include "engine/globals.h"

layout(set = SHADER_UNIFORM_SET, binding = 0) uniform Options {
    float4x4 culling_view;
    u32 instances_visibility_descriptor;
};

#define PREDICATE_BUFFER global_buffers_uint[instances_visibility_descriptor].data

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;
void main()
{
    uint local_idx  = gl_LocalInvocationIndex;
    uint global_idx = gl_GlobalInvocationID.x;

    if (global_idx >= globals.submesh_instances_count)
    {
        return;
    }

    SubMeshInstance submesh_instance = get_submesh_instance(global_idx);
    RenderInstance instance          = get_render_instance(submesh_instance.i_instance);
    RenderMesh mesh                  = get_render_mesh(submesh_instance.i_mesh);
    SubMesh submesh                  = get_submesh(mesh.first_submesh, submesh_instance.i_submesh);

    BVHNode bvh_root = get_blas_node(mesh.bvh_root, 0);
    float4 corners[8] = {
        float4(bvh_root.bbox_min.x, bvh_root.bbox_min.y, bvh_root.bbox_min.z, 1.0),
        float4(bvh_root.bbox_min.x, bvh_root.bbox_min.y, bvh_root.bbox_max.z, 1.0),
        float4(bvh_root.bbox_min.x, bvh_root.bbox_max.y, bvh_root.bbox_min.z, 1.0),
        float4(bvh_root.bbox_min.x, bvh_root.bbox_max.y, bvh_root.bbox_max.z, 1.0),
        float4(bvh_root.bbox_max.x, bvh_root.bbox_min.y, bvh_root.bbox_min.z, 1.0),
        float4(bvh_root.bbox_max.x, bvh_root.bbox_min.y, bvh_root.bbox_max.z, 1.0),
        float4(bvh_root.bbox_max.x, bvh_root.bbox_max.y, bvh_root.bbox_min.z, 1.0),
        float4(bvh_root.bbox_max.x, bvh_root.bbox_max.y, bvh_root.bbox_max.z, 1.0)
    };

    // transform the mesh AABB in worldspace
    for (u32 i_corner = 0; i_corner < 8; i_corner += 1)
    {
        corners[i_corner] = globals.camera_projection * culling_view * instance.object_to_world * corners[i_corner];
    }

    bool outside_left_plane = true;
    for (u32 i_corner = 0; i_corner < 8; i_corner += 1)
    {
        outside_left_plane = outside_left_plane && (corners[i_corner].x < -corners[i_corner].w);
    }

    bool outside_right_plane = true;
    for (u32 i_corner = 0; i_corner < 8; i_corner += 1)
    {
        outside_right_plane = outside_right_plane && (corners[i_corner].x > corners[i_corner].w);
    }

    bool outside_bottom_plane = true;
    for (u32 i_corner = 0; i_corner < 8; i_corner += 1)
    {
        outside_bottom_plane = outside_bottom_plane && (corners[i_corner].y < -corners[i_corner].w);
    }

    bool outside_top_plane = true;
    for (u32 i_corner = 0; i_corner < 8; i_corner += 1)
    {
        outside_top_plane = outside_top_plane && (corners[i_corner].y > corners[i_corner].w);
    }

    bool outside_near_plane = true;
    for (u32 i_corner = 0; i_corner < 8; i_corner += 1)
    {
        outside_near_plane = outside_near_plane && (corners[i_corner].z < 0);
    }

    bool outside = outside_left_plane || outside_right_plane || outside_bottom_plane || outside_top_plane || outside_near_plane;
    bool visible = outside == false;
    PREDICATE_BUFFER[global_idx] = u32(visible);
}
