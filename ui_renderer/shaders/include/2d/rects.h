// -*- mode: glsl; -*-
#ifndef _2D_RECTS_H
#define _2D_RECTS_H

#include "base/types.h"
#include "base/constants.h"
#include "base/bindless.h"

struct Rect
{
    float2 position;
    float2 size;
};
const u32 sizeof_rect = sizeof_float4;

struct ColorRect
{
    Rect rect;
    u32 color;
    u32 i_clip_rect;
    u32 padding[2];
};
const u32 sizeof_color_rect = 2 * sizeof_float4;

struct SdfRect
{
    Rect rect;
    u32 color;
    u32 i_clip_rect;
    u32 border_color;
    u32 border_thickness;
};
const u32 sizeof_sdf_rect = 2 * sizeof_float4;

struct TexturedRect
{
    Rect rect;
    Rect uv;
    u32 texture_descriptor;
    u32 i_clip_rect;
    u32 padding[2];
};
const u32 sizeof_textured_rect = 3 * sizeof_float4;

const u32 RectType_Color = 0;
const u32 RectType_Textured = 1;
const u32 RectType_Clip = 2;
const u32 RectType_Sdf_RoundRectangle = 32;
const u32 RectType_Sdf_Circle = 33;


bool is_sdf_type(u32 primitive_type)
{
    return (primitive_type & 32) != 0;
}

#define BINDLESS_BUFFER layout(set = GLOBAL_BINDLESS_SET, binding = GLOBAL_BUFFER_BINDING) buffer

BINDLESS_BUFFER ColorRectBuffer    { ColorRect rects[];  } global_buffers_color_rects[];
BINDLESS_BUFFER TexturedRectBuffer { TexturedRect rects[];  } global_buffers_textured_rects[];
BINDLESS_BUFFER RectBuffer         { Rect rects[];  } global_buffers_rects[];
BINDLESS_BUFFER SdfBuffer          { SdfRect rects[];  } global_buffers_sdf_rects[];

#endif
