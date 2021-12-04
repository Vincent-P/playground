#pragma shader_stage(compute)

#include "types.h"
#include "constants.h"
#include "globals.h"

layout(set = SHADER_SET, binding = 0) uniform Options {
    u32 draw_arguments_descriptor;
};

#define DRAW_COUNT global_buffers_draw_arguments[draw_arguments_descriptor].draw_count
#define DRAW_ARGUMENTS_BUFFER global_buffers_draw_arguments[draw_arguments_descriptor].arguments

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;
void main()
{
    uint local_idx  = gl_LocalInvocationIndex;
    uint global_idx = gl_GlobalInvocationID.x;

    if (global_idx == 0)
    {
        DRAW_COUNT = 0;
    }
    barrier();

    if (global_idx >= globals.submesh_instances_count)
    {
        return;
    }

    SubMeshInstance submesh_instance = get_submesh_instance(global_idx);
    RenderInstance instance          = get_render_instance(submesh_instance.i_instance);
    RenderMesh mesh                  = get_render_mesh(submesh_instance.i_mesh);
    SubMesh submesh = get_submesh(mesh.first_submesh, submesh_instance.i_submesh);

    u32 i_draw = submesh_instance.i_draw;

    atomicAdd(DRAW_COUNT, 1);
    DRAW_ARGUMENTS_BUFFER[i_draw].vertex_count    = submesh.index_count;
    DRAW_ARGUMENTS_BUFFER[i_draw].instance_count  = 0;
    DRAW_ARGUMENTS_BUFFER[i_draw].index_offset    = mesh.first_index + submesh.first_index;
    DRAW_ARGUMENTS_BUFFER[i_draw].vertex_offset   = 0;
    DRAW_ARGUMENTS_BUFFER[i_draw].instance_offset = ~0u;
}
