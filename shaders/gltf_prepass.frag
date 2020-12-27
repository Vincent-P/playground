#include "globals.h"
#include "pbr.h"

layout(set = 1, binding = 1) buffer Materials {
    Material materials[];
};

layout(set = 1, binding = 2) buffer DrawDatas {
    DrawData draw_datas[];
};

layout (location = 2) in float2 i_uv0;
layout (location = 4) in float4 i_color0;
layout (location = 7) in flat int i_drawid;

void main()
{
    DrawData current_draw = draw_datas[i_drawid];
    Material material = materials[current_draw.material_idx];
    float4 base_color = get_base_color(material, i_uv0) * i_color0;
    if (base_color.a < 0.25) {
        discard;
    }
}
