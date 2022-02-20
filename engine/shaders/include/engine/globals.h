// -*- mode: glsl; -*-

#ifndef GLOBALS_H
#define GLOBALS_H

#include "base/types.h"
#include "base/bindless.h"

/// --- Structures

struct RenderMesh
{
    u32 first_position;
    u32 first_index;
    u32 first_submesh;
    u32 bvh_root;
    u32 first_uv;
    u32 pad00;
    u32 pad01;
    u32 pad10;
};

struct RenderInstance
{
    float4x4 object_to_world;
    float4x4 world_to_object;
    u32 i_render_mesh;
    u32 pad00;
    u32 pad01;
    u32 pad10;
};

struct SubMeshInstance
{
    u32 i_mesh;
    u32 i_submesh;
    u32 i_instance;
    u32 i_draw;
};

struct SubMesh
{
    u32 first_index;
    u32 first_vertex;
    u32 index_count;
    u32 i_material;
};

struct ImGuiVertex
{
    float2 position;
    float2 uv;
    uint color;
    uint pad00;
    uint pad01;
    uint pad10;
};

struct FontVertex
{
    ivec2 position;
    vec2 uv;
};

struct BVHNode
{
    float3 bbox_min;
    u32    prim_index;
    float3 bbox_max;
    u32    next_node;
};

struct DrawIndexedOptions
{
    u32 vertex_count;
    u32 instance_count;
    u32 index_offset;
    i32 vertex_offset;
    u32 instance_offset;
};


struct Material
{
    float4 base_color_factor;
    float4 emissive_factor;
    float metallic_factor;
    float roughness_factor;
    u32 base_color_texture;
    u32 normal_texture;
    u32 metallic_roughness_texture;
    float rotation;
    float2 offset;
    float2 scale;
    float2 pad00;
};

/// --- Global bindings

layout(push_constant) uniform PushConstants {
    u32 draw_id;
    u32 gui_texture_id;
} push_constants;

layout(set = GLOBAL_UNIFORM_SET, binding = 0) uniform GlobalUniform {
    float4x4 camera_view;
    float4x4 camera_projection;
    float4x4 camera_view_inverse;
    float4x4 camera_projection_inverse;
    float4x4 camera_previous_view;
    float4x4 camera_previous_projection;

    float2 render_resolution;
    float2 jitter_offset;

    float delta_t;
    u32 frame_count;
    bool enable_taa;
    u32 meshes_data_descriptor;

    u32 instances_data_descriptor;
    u32 instances_offset;
    u32 submesh_instances_data_descriptor;
    u32 submesh_instances_offset;

    u32 tlas_descriptor;
    u32 submesh_instances_count;
    u32 index_buffer_descriptor;
    u32 vertex_positions_descriptor;

    u32 bvh_nodes_descriptor;
    u32 submeshes_descriptor;
    u32 culled_instances_indices_descriptor;
    u32 materials_descriptor;

    u32 vertex_uvs_descriptor;
    bool enable_hbao;
    u32 hbao_samples_per_dir;
    u32 hbao_dir_count;

    u32 hbao_radius;
} globals;

#define BINDLESS_BUFFER layout(set = GLOBAL_BINDLESS_SET, binding = GLOBAL_BUFFER_BINDING) buffer

BINDLESS_BUFFER UiVerticesBuffer            { ImGuiVertex vertices[];  } global_buffers_ui_vert[];
BINDLESS_BUFFER FontVerticesBuffer          { FontVertex vertices[];  } global_buffers_font_vert[];
BINDLESS_BUFFER InstancesBuffer             { RenderInstance render_instances[]; } global_buffers_instances[];
BINDLESS_BUFFER MeshesBuffer                { RenderMesh render_meshes[]; } global_buffers_meshes[];
BINDLESS_BUFFER SubMeshInstancesBuffer      { SubMeshInstance submesh_instances[]; } global_buffers_submesh_instances[];
BINDLESS_BUFFER SubMeshesBuffer             { SubMesh submeshes[]; } global_buffers_submeshes[];
BINDLESS_BUFFER PositionsBuffer             { float4 positions[]; } global_buffers_positions[];
BINDLESS_BUFFER UvsBuffer                   { float2 uvs[]; } global_buffers_uvs[];
BINDLESS_BUFFER IndicesBuffer               { u32 indices[]; } global_buffers_indices[];
BINDLESS_BUFFER BVHBuffer                   { BVHNode nodes[]; } global_buffers_bvh[];
BINDLESS_BUFFER DrawArgumentsBuffer         { u32 draw_count; DrawIndexedOptions arguments[]; } global_buffers_draw_arguments[];
BINDLESS_BUFFER UintBuffer                  { u32 data[]; } global_buffers_uint[];
BINDLESS_BUFFER MaterialsBuffer             { Material materials[]; } global_buffers_materials[];

/// --- Accessors

RenderInstance get_render_instance(u32 i_instance)
{
    return global_buffers_instances[globals.instances_data_descriptor].render_instances[globals.instances_offset + i_instance];
}

RenderMesh get_render_mesh(u32 i_mesh)
{
    return global_buffers_meshes[globals.meshes_data_descriptor].render_meshes[i_mesh];
}

u32 get_index(u32 i_index)
{
    return global_buffers_indices[globals.index_buffer_descriptor].indices[i_index];
}

float4 get_position(u32 first_position, u32 i_position)
{
    return global_buffers_positions[globals.vertex_positions_descriptor].positions[first_position + i_position];
}

float2 get_uv(u32 first_uv, u32 i_uv)
{
    return global_buffers_uvs[globals.vertex_uvs_descriptor].uvs[first_uv + i_uv];
}

BVHNode get_tlas_node(u32 i_node)
{
    return global_buffers_bvh[globals.tlas_descriptor].nodes[i_node];
}

BVHNode get_blas_node(u32 bvh_root, u32 i_node)
{
    return global_buffers_bvh[globals.bvh_nodes_descriptor].nodes[bvh_root + i_node];
}

SubMeshInstance get_submesh_instance(u32 i_submesh_instance)
{
    return global_buffers_submesh_instances[globals.submesh_instances_data_descriptor].submesh_instances[globals.submesh_instances_offset + i_submesh_instance];
}

SubMesh get_submesh(u32 first_submesh, u32 i_submesh)
{
    return global_buffers_submeshes[globals.submeshes_descriptor].submeshes[first_submesh + i_submesh];
}

Material get_material(u32 i_material)
{
    return global_buffers_materials[globals.materials_descriptor].materials[i_material];
}
#endif
