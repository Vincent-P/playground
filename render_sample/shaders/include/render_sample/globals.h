// -*- mode: glsl; -*-

#ifndef GLOBALS_H
#define GLOBALS_H

#include "base/types.h"
#include "base/constants.h"
#include "base/bindless.h"

struct ImGuiVertex
{
    float2 position;
    float2 uv;
    uint color;
    uint pad00;
    uint pad01;
    uint pad10;
};

struct Rect
{
    float2 position;
    float2 size;
};

struct ColorRect
{
    Rect rect;
    u32 color;
    u32 padding[3];
};
const u32 sizeof_color_rect = 3 * sizeof_float4;

struct TexturedRect
{
    Rect rect;
    Rect uv;
    u32 texture_descriptor;
    u32 padding[3];
};
const u32 sizeof_textured_rect = 3 * sizeof_float4;

const u32 RectType_Color = 0;
const u32 RectType_Textured = 1;

layout(push_constant) uniform PushConstants {
    u32 draw_id;
    u32 gui_texture_id;
} push_constants;

layout(set = GLOBAL_UNIFORM_SET, binding = 0) uniform GlobalUniform {
    u32 color_rect_buffer_descriptor;
    u32 textured_rect_buffer_descriptor;
} globals;

layout(set = GLOBAL_BUFFER_SET, binding = 0) buffer UiVerticesBuffer   { ImGuiVertex vertices[];  } global_buffers_ui_vert[];
layout(set = GLOBAL_BUFFER_SET, binding = 0) buffer ColorRectBuffer    { ColorRect rects[];  } global_buffers_color_rects[];
layout(set = GLOBAL_BUFFER_SET, binding = 0) buffer TexturedRectBuffer { TexturedRect rects[];  } global_buffers_textured_rects[];

#endif
