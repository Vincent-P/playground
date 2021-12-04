#pragma shader_stage(fragment)

#include "types.h"
#include "globals.h"

layout(set = SHADER_SET, binding = 0) uniform Options {
    float2 scale;
    float2 translation;
    u64 vertices_ptr_ptr;
    u32 first_vertex;
    u32 pad4;
};

layout(location = 0) in float2 i_uv;
layout(location = 1) in float4 i_color;
layout(location = 0) out float4 o_color;

void main()
{
    o_color = texture(global_textures[push_constants.gui_texture_id], i_uv) * i_color;
    o_color.rgb *= o_color.a;
}
