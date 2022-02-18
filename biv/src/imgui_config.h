#pragma once
#include "exo/maths/numerics.h"
#include "exo/maths/vectors.h"
#include "exo/macros/packed.h"

//---- Don't define obsolete functions/enums/behaviors. Consider enabling from time to time after updating to avoid using soon-to-be obsolete function/names.
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS

//---- Define constructor and implicit cast operators to convert back<>forth between your math types and ImVec2/ImVec4.
// This will be inlined as part of ImVec2 and ImVec4 class declarations.
#define IM_VEC2_CLASS_EXTRA                                                 \
    ImVec2(const exo::float2& f) { x = f.x; y = f.y; }                  \
    operator exo::float2() const { return exo::float2(x,y); }

#define IM_VEC4_CLASS_EXTRA                                                 \
    ImVec4(const exo::float4& f) { x = f.x; y = f.y; z = f.z; w = f.w; } \
    operator exo::float4() const { return exo::float4(x,y,z,w); }

#define ImDrawIdx u16

#define IMGUI_OVERRIDE_DRAWVERT_STRUCT_LAYOUT \
PACKED(struct ImDrawVert                  \
{ \
    ImVec2  pos; \
    ImVec2  uv; \
    ImU32   col; \
    u32 pad00; \
    u32 pad01;    \
    u32 pad10;       \
})
