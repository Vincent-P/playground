#extension GL_EXT_nonuniform_qualifier : require

#include "globals.h"
#include "pbr.h"

layout (location = 0) in float2 i_uv;
layout (location = 1) in float4 i_color;

void main()
{
    float4 base_color = get_base_color(i_uv) * i_color;
    if (base_color.a < 0.5) {
        discard;
    }
}
