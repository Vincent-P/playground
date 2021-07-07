#include "types.h"
#include "globals.h"
#include "imgui.h"

layout(location = 0) in float2 i_uv;
layout(location = 1) in float4 i_color;
layout(location = 0) out float4 o_color;

void main()
{
    uint texture_id = texture_binding_per_draw[push_constants.draw_idx/4][push_constants.draw_idx%4];
    o_color = texture(global_textures[texture_id], i_uv) * i_color;
    o_color.rgb *= o_color.a;
}
