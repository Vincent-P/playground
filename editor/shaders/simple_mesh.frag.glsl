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



#define TEXTURE_BOUND(i) i != u32_invalid
#define TEXTURE_VALID(i) i < 128
#define TEXTURE_INVALID(i) i >= 128

void main()
{
	MaterialDescriptor material = global_buffers_materials[materials_descriptor].materials[i_material_index];

	const float4 error = float4(4.0 - mod(gl_FragCoord.x + gl_FragCoord.y, 10.0), 0, 0, 1.0);

#define ERROR \
	o_color = error; \
	gl_FragDepth = 1.0; \
	return;

	if (i_material_index > 43) {
		ERROR
	}

	float4 base_color = i_base_color;
	if (TEXTURE_BOUND(material.base_color_texture)) {
		if (TEXTURE_VALID(material.base_color_texture)) {
			base_color = base_color * texture(global_textures[nonuniformEXT(material.base_color_texture)], i_uvs);
		}
		else {
			ERROR
		}
	}

	if (base_color.a < 0.5) {
		discard;
	}

	float4 normal = float4(0, 0, 1, 1);
	if (TEXTURE_BOUND(material.normal_texture)) {
		if (TEXTURE_VALID(material.normal_texture)) {
			normal = texture(global_textures[nonuniformEXT(material.normal_texture)], i_uvs);
		}
		else {
			ERROR
		}
	}

	float4 metallic_roughness = float4(0.0);
	if (TEXTURE_BOUND(material.metallic_roughness_texture)) {
		if (TEXTURE_VALID(material.metallic_roughness_texture)) {
			metallic_roughness = texture(global_textures[nonuniformEXT(material.metallic_roughness_texture)], i_uvs);
		}
		else {
			ERROR
		}
	}

	o_color.rgb = 0.33 * (base_color.rgb + normal.rgb + metallic_roughness.rgb);
	o_color.rgb = base_color.rgb;
	gl_FragDepth = gl_FragCoord.z;
}
