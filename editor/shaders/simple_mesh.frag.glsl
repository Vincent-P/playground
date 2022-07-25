#pragma shader_stage(fragment)

#include "base/types.h"
#include "base/constants.h"
#include "editor/mesh.h"

layout(set = SHADER_UNIFORM_SET, binding = 0) uniform Options {
    float4x4 view;
    float4x4 projection;
    u32 instances_descriptor;
    u32 meshes_descriptor;
    u32 i_submesh;
    u32 materials_descriptor;
};

layout(location = 0) in float4 i_world_pos;
layout(location = 1) in float4 i_base_color;
layout(location = 2) in float2 i_uvs;
layout(location = 3) in flat uint i_material_index;
layout(location = 0) out float4 o_color;
void main()
{
    MaterialDescriptor material = global_buffers_materials[materials_descriptor].materials[i_material_index];

    float4 base_color = i_base_color;
    if (material.base_color_texture != u32_invalid) {
	    base_color = base_color * texture(global_textures[nonuniformEXT(material.base_color_texture)], i_uvs);
    }

    o_color.rgb = base_color.rgb;
    o_color.a = 1.0;
}
