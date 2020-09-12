#include "globals.h"
#include "pbr.h"

#extension GL_EXT_nonuniform_qualifier : require

layout (location = 2) in vec2 inUV0;

void main()
{
    vec4 base_color = texture(global_textures[nonuniformEXT(constants.base_color_idx)], inUV0);
    if (base_color.a < 0.5) {
        discard;
    }
}
