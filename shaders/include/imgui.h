#ifndef IMGUI_HEADER
#define IMGUI_HEADER
#include "types.h"

struct ImGuiVertex
{
    float2 position;
    float2 uv;
    uint color;
    uint pad00;
    uint pad01;
    uint pad10;
};

#if 0
layout(buffer_reference) buffer VerticesType {
    ImGuiVertex vertices[];
};
#endif

layout(set = 3, binding = 1) buffer VerticesBuffer {
    ImGuiVertex vertices[];
} vertices_ptr;

layout(set = 3, binding = 0) uniform Options {
    float2 scale;
    float2 translation;
    #if 0
    VerticesType vertices_ptr_ptr;
    #else
    u32 pad0;
    u32 pad1;
    #endif
    u32 first_vertex;
    u32 pad4;
    uvec4 texture_binding_per_draw[64/4];
};

#endif
