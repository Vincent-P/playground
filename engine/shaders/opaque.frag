#version 460

#include "globals.h"
#include "hash.h"

layout(location = 0) flat in uint i_instance_index;
layout(location = 0) out vec4 o_color;
void main()
{
    uint id = uint(gl_PrimitiveID);
    vec2 seed = vec2(id % 256, id / 256);
    vec3 color = hash_color(hash(seed));
    o_color = vec4(color, 1.0);
}
