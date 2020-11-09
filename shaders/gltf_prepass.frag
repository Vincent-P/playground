#include "globals.h"
#include "pbr.h"

#extension GL_EXT_nonuniform_qualifier : require

layout (location = 2) in float2 i_uv;

void main()
{
    float4 base_color = get_base_color(i_uv);
    if (base_color.a < 0.5) {
        discard;
    }
}
