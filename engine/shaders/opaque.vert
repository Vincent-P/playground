#include "types.h"
#include "globals.h"

layout(location = 0) flat out uint o_instance_index;
void main()
{
    u32 i_submesh_instance = global_buffers_uint[globals.culled_instances_indices_descriptor].data[gl_InstanceIndex];
    SubMeshInstance submesh_instance = get_submesh_instance(i_submesh_instance);
    RenderInstance instance = get_render_instance(submesh_instance.i_instance);
    RenderMesh mesh = get_render_mesh(submesh_instance.i_mesh);

    u32    index    = gl_VertexIndex;
    float4 position = get_position(mesh.first_position, index);


    float4x4 projection = globals.camera_projection;
    projection[2][0] = globals.jitter_offset.x / globals.render_resolution.x;
    projection[2][1] = globals.jitter_offset.y / globals.render_resolution.y;
    gl_Position = projection * globals.camera_view * instance.object_to_world * float4(position.xyz, 1.0);
    o_instance_index = gl_InstanceIndex;
}
