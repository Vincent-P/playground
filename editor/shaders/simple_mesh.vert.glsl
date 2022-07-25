#pragma shader_stage(vertex)

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

layout(location = 0) out float4 o_world_pos;
layout(location = 1) out float4 o_base_color;
layout(location = 2) out float2 o_uvs;
layout(location = 3) out flat uint o_material_index;
void main()
{
    u32 i_instance = u32(gl_InstanceIndex);
    InstanceDescriptor instance = global_buffers_instances[instances_descriptor].instances[i_instance];
    MeshDescriptor mesh = global_buffers_meshes[meshes_descriptor].meshes[instance.i_mesh_descriptor];

    SubmeshDescriptor submesh = global_buffers_submeshes[nonuniformEXT(mesh.submesh_buffer_descriptor)].submeshes[i_submesh];
    MaterialDescriptor material = global_buffers_materials[materials_descriptor].materials[submesh.i_material];

    float4 vertex = global_buffers_positions[nonuniformEXT(mesh.positions_buffer_descriptor)].positions[gl_VertexIndex];
    float2 uvs = global_buffers_uvs[nonuniformEXT(mesh.uvs_buffer_descriptor)].uvs[gl_VertexIndex];
    uvs = uvs * material.scale + material.offset;

    o_world_pos = instance.transform * vertex;
    gl_Position = projection * (view * o_world_pos);
    o_base_color = material.base_color_factor;
    o_uvs = uvs;
    o_material_index = submesh.i_material;
}
