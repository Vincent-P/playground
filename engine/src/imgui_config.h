#pragma once
#include "base/types.hpp"

//---- Don't define obsolete functions/enums/behaviors. Consider enabling from time to time after updating to avoid using soon-to-be obsolete function/names.
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS

//---- Define constructor and implicit cast operators to convert back<>forth between your math types and ImVec2/ImVec4.
// This will be inlined as part of ImVec2 and ImVec4 class declarations.
#define IM_VEC2_CLASS_EXTRA                                                 \
    ImVec2(const float2& f) { x = f.x; y = f.y; }                     \
        operator float2() const { return float2(x,y); }

#define IM_VEC4_CLASS_EXTRA                                                 \
        ImVec4(const float4& f) { x = f.x; y = f.y; z = f.z; w = f.w; }     \
        operator float4() const { return float4(x,y,z,w); }

#define ImDrawIdx u16

#define IMGUI_OVERRIDE_DRAWVERT_STRUCT_LAYOUT \
struct PACKED ImDrawVert \
{ \
    ImVec2  pos; \
    ImVec2  uv; \
    ImU32   col; \
    unsigned pad00; \
    unsigned pad01; \
    unsigned pad10; \
}
