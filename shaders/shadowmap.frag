#extension GL_EXT_nonuniform_qualifier : require

#include "globals.h"
#include "pbr.h"

layout (location = 0) in vec2 inUV0;

void main()
{
    vec4 base_color = texture(global_textures[constants.base_color_idx], inUV0);
    if (base_color.a < 0.5) {
        discard;
    }
}
