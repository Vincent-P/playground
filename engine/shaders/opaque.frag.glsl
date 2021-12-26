#pragma shader_stage(fragment)

#include "engine/globals.h"

layout(location = 0) flat in uint i_instance_index;
layout(location = 1) flat in uint i_triangle_index;
layout(location = 0) out uint4 o_color;
void main()
{
    o_color = uint4(i_instance_index, gl_PrimitiveID, 0.0, 0.0);
}
