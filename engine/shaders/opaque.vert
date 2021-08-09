#include "types.h"
#include "globals.h"

layout(location = 0) flat out uint o_instance_index;
void main()
{
    RenderInstance instance = get_render_instance(gl_InstanceIndex);
    RenderMesh mesh = get_render_mesh(instance.i_render_mesh);

    u32    index    = gl_VertexIndex;
    float4 position = get_position(mesh.first_position, index);


    float4x4 projection = globals.camera_projection;
    projection[2][0] = globals.jitter_offset.x / globals.render_resolution.x;
    projection[2][1] = globals.jitter_offset.y / globals.render_resolution.y;
    gl_Position = projection * globals.camera_view * instance.object_to_world * float4(position.xyz, 1.0);
    o_instance_index = gl_InstanceIndex;
}
