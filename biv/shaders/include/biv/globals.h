// -*- mode: glsl; -*-

#ifndef GLOBALS_H
#define GLOBALS_H

#include "base/types.h"
#include "base/constants.h"
#include "base/bindless.h"
#include "2d/rects.h"

struct ImGuiVertex
{
    float2 position;
    float2 uv;
    uint color;
    uint pad00;
    uint pad01;
    uint pad10;
};

layout(push_constant) uniform PushConstants {
    u32 draw_id;
    u32 gui_texture_id;
} push_constants;

layout(set = GLOBAL_UNIFORM_SET, binding = 0) uniform GlobalUniform {
    u32 color_rect_buffer_descriptor;
    u32 textured_rect_buffer_descriptor;
} globals;

#define BINDLESS_BUFFER layout(set = GLOBAL_BINDLESS_SET, binding = GLOBAL_BUFFER_BINDING) buffer

BINDLESS_BUFFER UiVerticesBuffer   { ImGuiVertex vertices[];  } global_buffers_ui_vert[];
BINDLESS_BUFFER ColorRectBuffer    { ColorRect rects[];  } global_buffers_color_rects[];
BINDLESS_BUFFER TexturedRectBuffer { TexturedRect rects[];  } global_buffers_textured_rects[];
BINDLESS_BUFFER RectBuffer         { Rect rects[];  } global_buffers_rects[];
BINDLESS_BUFFER SdfBuffer          { SdfRect rects[];  } global_buffers_sdf_rects[];

#endif
