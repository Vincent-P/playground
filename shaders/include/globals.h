#ifndef GLOBALS_H
#define GLOBALS_H

// #extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_shader_draw_parameters : require

#include "types.h"

/// --- Structures

struct Vertex
{
    float3 position;
    float pad00;
    float3 normal;
    float pad01;
    float2 uv0;
    float2 uv1;
    float4 color0;
};

struct RenderMeshData
{
    float4x4 transform;
    u32 i_mesh;
    u32 gen_mesh;
    u32 i_material;
    u32 texture_offset;
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
    u32 pad00;
    u32 pad01;
    u32 pad10;
};

/// --- Global bindings

layout(set = 0, binding = 1) uniform sampler2D global_textures[];
layout(set = 0, binding = 1) uniform sampler3D global_textures_3d[];

layout(set = 0, binding = 0) uniform GlobalUniform {
    float4x4 camera_view;
    float4x4 camera_projection;
    float4x4 camera_view_inverse;
    float4x4 camera_projection_inverse;
    float4   camera_position;
    u32 pad00;
    u32 pad01;
    u32 pad10;
    u32 pad11;
    float2   resolution;
    float delta_t;
    u32 frame_count;
    u32 camera_moved;
} globals;

layout(push_constant) uniform PushConstants {
    u32 draw_idx;
    u32 render_mesh_idx;
} push_constants;

#endif
