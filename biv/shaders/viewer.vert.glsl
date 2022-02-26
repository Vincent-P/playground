#pragma shader_stage(vertex)

#include "base/types.h"
#include "base/constants.h"
#include "biv/globals.h"

layout(set = SHADER_UNIFORM_SET, binding = 0) uniform Options {
    float2 scale;
    float2 translation;
    u32 texture_descriptor;
    u32 viewer_flags;
};

const float2 quad_vertices[] = {
    float2(0, 0),
    float2(1, 0),
    float2(1, 1),

    float2(0, 0),
    float2(1, 1),
    float2(0, 1),
};


layout(location = 0) out float2 o_uv;
void main()
{
    const float2 vertex = quad_vertices[gl_VertexIndex % 6];
    gl_Position = float4(vertex * scale + translation, 0.0, 1.0);
    o_uv = vertex;
}
