#include "types.h"
#include "globals.h"

layout(set = SHADER_SET, binding = 0) uniform Options {
    u32 first_instance;
    u32 instances_descriptor;
    u32 meshes_descriptor;
};

void main()
{
    RenderInstance instance = global_buffers_instances[instances_descriptor].render_instances[first_instance + gl_InstanceIndex];
    RenderMesh mesh = global_buffers_meshes[meshes_descriptor].render_meshes[instance.i_render_mesh];

    u32    index    = global_buffers_indices[mesh.indices_descriptor].indices[gl_VertexIndex];
    float4 position = global_buffers_positions[mesh.positions_descriptor].positions[index];

    gl_Position = globals.camera_projection * globals.camera_view * instance.transform * float4(position.xyz, 1.0);
}
