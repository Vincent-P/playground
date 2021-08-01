#ifndef GLOBALS_H
#define GLOBALS_H

// #extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_shader_draw_parameters : require

#include "types.h"

/// --- Structures

struct RenderMesh
{
    u32 positions_descriptor;
    u32 indices_descriptor;
    u32 bvh_descriptor;
    u32 pad01;
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
    float2   resolution;
    float delta_t;
    u32 frame_count;
    float2 jitter_offset;
} globals;

layout(set = 1, binding = 0) uniform sampler2D global_textures[];
layout(set = 1, binding = 0) uniform sampler3D global_textures_3d[];

layout(set = 2, binding = 0, rgba8) uniform image2D global_images_2d_rgba8[];
layout(set = 2, binding = 0, rgba32f) uniform image2D global_images_2d_rgba32f[];
layout(set = 2, binding = 0, r32f) uniform image2D global_images_2d_r32f[];

layout(set = 3, binding = 0) buffer UiVerticesBuffer  { ImGuiVertex vertices[];  } global_buffers_ui_vert[];
layout(set = 3, binding = 0) buffer InstancesBuffer   { RenderInstance render_instances[]; } global_buffers_instances[];
layout(set = 3, binding = 0) buffer MeshesBuffer      { RenderMesh render_meshes[]; } global_buffers_meshes[];
layout(set = 3, binding = 0) buffer PositionsBuffer   { float4 positions[]; } global_buffers_positions[];
layout(set = 3, binding = 0) buffer IndicesBuffer     { u32 indices[]; } global_buffers_indices[];
layout(set = 3, binding = 0) buffer BVHBuffer         { BVHNode nodes[]; } global_buffers_bvh[];

#define SHADER_SET 4

#endif
