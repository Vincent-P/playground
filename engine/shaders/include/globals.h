#ifndef GLOBALS_H
#define GLOBALS_H

// #extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_shader_draw_parameters : require

#include "types.h"

/// --- Structures

struct RenderInstance
{
    float4x4 transform;
    u32 i_render_mesh;
    u32 pad00;
    u32 pad01;
    u32 pad10;
};

/// --- Global bindings

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

struct UIVert
{
    float2 position;
    float2 uv;
    uint color;
    uint pad00;
    uint pad01;
    uint pad10;
};

layout(set = 1, binding = 1) buffer global_ui_vert {
    UIVert vertices[];
} global_buffers_ui_vert[];

layout(set = 1, binding = 1) buffer global_position {
    float4 positions[];
} global_buffers_positions[];

layout(set = 2, binding = 0, rgba8) uniform image2D global_images_2d_rgba8[];
layout(set = 2, binding = 0, rgba32f) uniform image2D global_images_2d_rgba32f[];
layout(set = 2, binding = 0, r32f) uniform image2D global_images_2d_r32f[];

layout(push_constant) uniform PushConstants {
    u32 draw_id;
    u32 gui_texture_id;
} push_constants;

#endif
