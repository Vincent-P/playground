#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require

#include "types.h"
#include "globals.h"

struct ImGuiVertex
{
    float2 position;
    float2 uv;
    uint color;
    uint pad0;
};

layout(buffer_reference) buffer VerticesType {
    ImGuiVertex vertices[];
};

layout(set = 1, binding = 0) uniform Options {
    float2 scale;
    float2 translation;
    VerticesType vertices_ptr;
    uint texture_binding;
};

layout(location = 0) in float2 i_uv;
layout(location = 1) in float4 i_color;
layout(location = 0) out float4 o_color;
void main()
{
    o_color = texture(global_textures[texture_binding], i_uv) * i_color;
    o_color.rgb *= o_color.a;
}
