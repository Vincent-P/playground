// -*- mode: glsl; -*-

#ifndef GLOBALS_H
#define GLOBALS_H

// #extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_shader_draw_parameters : require

#include "types.h"

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


layout(set = 0, binding = 0) uniform GlobalUniform {
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
    u32 first_accumulation_frame;
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
} globals;

layout(set = 1, binding = 0) uniform sampler2D global_textures[];
layout(set = 1, binding = 0) uniform usampler2D global_textures_uint[];
layout(set = 1, binding = 0) uniform sampler3D global_textures_3d[];
layout(set = 1, binding = 0) uniform usampler3D global_textures_3d_uint[];

layout(set = 2, binding = 0, rgba8) uniform image2D global_images_2d_rgba8[];
layout(set = 2, binding = 0, rgba16f) uniform image2D global_images_2d_rgba16f[];
layout(set = 2, binding = 0, rgba32f) uniform image2D global_images_2d_rgba32f[];
layout(set = 2, binding = 0, r32f) uniform image2D global_images_2d_r32f[];

layout(set = 3, binding = 0) buffer UiVerticesBuffer            { ImGuiVertex vertices[];  } global_buffers_ui_vert[];
layout(set = 3, binding = 0) buffer InstancesBuffer             { RenderInstance render_instances[]; } global_buffers_instances[];
layout(set = 3, binding = 0) buffer MeshesBuffer                { RenderMesh render_meshes[]; } global_buffers_meshes[];
layout(set = 3, binding = 0) buffer SubMeshInstancesBuffer      { SubMeshInstance submesh_instances[]; } global_buffers_submesh_instances[];
layout(set = 3, binding = 0) buffer SubMeshesBuffer             { SubMesh submeshes[]; } global_buffers_submeshes[];
layout(set = 3, binding = 0) buffer PositionsBuffer             { float4 positions[]; } global_buffers_positions[];
layout(set = 3, binding = 0) buffer UvsBuffer                   { float2 uvs[]; } global_buffers_uvs[];
layout(set = 3, binding = 0) buffer IndicesBuffer               { u32 indices[]; } global_buffers_indices[];
layout(set = 3, binding = 0) buffer BVHBuffer                   { BVHNode nodes[]; } global_buffers_bvh[];
layout(set = 3, binding = 0) buffer DrawArgumentsBuffer         { u32 draw_count; DrawIndexedOptions arguments[]; } global_buffers_draw_arguments[];
layout(set = 3, binding = 0) buffer UintBuffer                  { u32 data[]; } global_buffers_uint[];
layout(set = 3, binding = 0) buffer MaterialsBuffer             { Material materials[]; } global_buffers_materials[];

#define SHADER_SET 4

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
