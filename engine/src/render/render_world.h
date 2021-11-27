#pragma once
#include <exo/prelude.h>
#include <exo/maths/matrices.h>
#include <exo/maths/aabb.h>
#include <exo/collections/vector.h>
#include <exo/collections/map.h>
#include <exo/cross/uuid.h>

struct DrawableInstance
{
    cross::UUID mesh_asset;
    float4x4 world_transform;
    AABB world_bounds;
};

struct MeshInstance
{
    // BVHNode bvh_root = {};
    Vec<u32> instances;
    Vec<u32> materials;
    u32 first_instance = 0;
};

// Description of the world that the renderer will use
struct RenderWorld
{
    // input
    float4x4 main_camera_view;
    float4x4 main_camera_view_inverse;
    float4x4 main_camera_projection;
    float4x4 main_camera_projection_inverse;

    Vec<DrawableInstance> drawable_instances;

    // intermediate result
    Map<cross::UUID, MeshInstance> mesh_instances;
};


/**

renderer prepare materials:
   for each materials:
     if material.base_color_texture is uploaded:
       material_gpu.base_color_texture = texture descriptor
     else:
       material_gpu.base_color_texture = invalid


renderer prepare geometry:

   # gather uploaded instances
   for each drawable:
     if not uploaded to gpu:
       upload to gpu
     else:
       push drawable to instance list
       push instance to mesh's instance list

   # gather all instances from all meshes in order
   for each uploaded mesh:
     if mesh has no instances:
       skip

     gpu_mesh.first_instance = render_mesh.instances[0]
     for each gpu mesh instances:
        for each submesh:
          submesh_instances.push(new instance for this submesh)

     draw_count += gpu_mesh.submeshes.size()

   upload instance list (transforms, etc)
   upload submesh instance list (material id, etc)

   build_TLAS(instances)
   upload TLAS


 **/
