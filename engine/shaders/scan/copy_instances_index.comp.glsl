#pragma shader_stage(compute)

#include "types.h"
#include "constants.h"
#include "globals.h"

layout(set = SHADER_SET, binding = 0) uniform Options {
    u32 predicate_descriptor;
    u32 scanned_indices_descriptor;
    u32 reduction_group_sum_descriptor;
    u32 instances_index_descriptor;
    u32 draw_arguments_descriptor;
};

#define SCANNED_INDICES_BUFFER global_buffers_uint[scanned_indices_descriptor].data
#define PREDICATE_BUFFER       global_buffers_uint[predicate_descriptor].data
#define GROUP_SUM_BUFFER       global_buffers_uint[reduction_group_sum_descriptor].data
#define INSTANCES_INDEX_BUFFER global_buffers_uint[instances_index_descriptor].data
#define DRAW_ARGUMENTS_BUFFER global_buffers_draw_arguments[draw_arguments_descriptor].arguments

layout(local_size_x = 128, local_size_y = 1, local_size_z = 1) in;
void main()
{
    uint local_idx  = gl_LocalInvocationIndex;
    uint global_idx = gl_GlobalInvocationID.x;
    uint group_id   = gl_WorkGroupID.x;

    u32 group_sum = group_id > 0 ? GROUP_SUM_BUFFER[group_id] : 0;
    u32 index = group_sum + SCANNED_INDICES_BUFFER[global_idx];

    if (PREDICATE_BUFFER[global_idx] != 0)
    {
        INSTANCES_INDEX_BUFFER[index] = global_idx;

        // global_idx -> i_submesh_instance
        // index -> instance offset

        u32 i_submesh_instance = global_idx;
        SubMeshInstance submesh_instance = get_submesh_instance(global_idx);
        atomicAdd(DRAW_ARGUMENTS_BUFFER[submesh_instance.i_draw].instance_count, 1);
        atomicMin(DRAW_ARGUMENTS_BUFFER[submesh_instance.i_draw].instance_offset, index);
    }
}
