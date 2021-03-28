#ifndef GLOBALS_H
#define GLOBALS_H

// #extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_shader_draw_parameters : require

#include "types.h"

// Bindless sampled textures
layout(set = 0, binding = 1) uniform sampler2D global_textures[];
layout(set = 0, binding = 1) uniform sampler3D global_textures_3d[];

// Bindless storage images
// layout(set = 0, binding = 2, rgba16) uniform image2D global_images_rgba16[];
// layout(set = 0, binding = 2, rgba8) uniform image2D global_images_rgba8[];
// layout(set = 0, binding = 2, r32f) uniform image2D global_images_r32f[];


// Global buffers are device addresses
struct PACKED glTFVertex
{
    float3 position;
    float pad00;
    float3 normal;
    float pad01;
    float2 uv0;
    float2 uv1;
    float4 color0;
    float4 joint0;
    float4 weight0;
};

struct PACKED glTFPrimitive
{
    u32 material;
    u32 first_index;
    u32 first_vertex;
    u32 index_count;
    float3 aab_min;
    u32 rendering_mode;
    float3 aab_max;
    u32 pad00;
};

#if 0
layout(buffer_reference) buffer glTFVertexBufferType {
    glTFVertex vertices[];
};

layout(buffer_reference) buffer glTFPrimitiveBufferType {
    glTFPrimitive primitives[];
};
#endif

layout(set = 0, binding = 0) uniform GlobalUniform {
    float4x4 camera_view;
    float4x4 camera_projection;
    float4x4 camera_view_inverse;
    float4x4 camera_projection_inverse;
    float4   camera_position;
    #if 0
    glTFVertexBufferType vertex_buffer_ptr;
    glTFPrimitiveBufferType primitive_buffer_ptr;
    #else
    u32 pad00;
    u32 pad01;
    u32 pad10;
    u32 pad11;
    #endif
    float2   resolution;
    float delta_t;
    u32 frame_count;
} globals;

layout(push_constant) uniform PushConstants {
    u32 draw_idx;
} push_constants;

#endif
