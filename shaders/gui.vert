#include "types.h"
#include "globals.h"
#include "imgui.h"

layout(location = 0) out float2 o_uv;
layout(location = 1) out float4 o_color;
void main()
{
    ImGuiVertex vertex = vertices_ptr.vertices[gl_VertexIndex];

    gl_Position = float4( vertex.position * scale + translation, 0.0, 1.0 );
    o_uv        = vertex.uv;
    o_color     = unpackUnorm4x8(vertex.color);
}
