#pragma shader_stage(compute)

#include "base/types.h"
#include "base/constants.h"
#include "engine/globals.h"

layout(set = SHADER_SET, binding = 0) uniform Options {
    u32 input_descriptor;
    u32 output_descriptor;
    u32 reduction_group_sum_descriptor;
};

#define INPUT_BUFFER global_buffers_uint[input_descriptor].data
#define OUTPUT_BUFFER global_buffers_uint[output_descriptor].data
#define GROUP_SUM_BUFFER global_buffers_uint[reduction_group_sum_descriptor].data

shared u32 temp[128];

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
void main()
{
    uint local_idx  = gl_LocalInvocationIndex;
    uint global_idx = gl_GlobalInvocationID.x;
    uint group_id   = gl_WorkGroupID.x;

    u32 offset = 1;

    temp[2 * local_idx]       = INPUT_BUFFER[2 * global_idx];
    temp[(2 * local_idx) + 1] = INPUT_BUFFER[(2 * global_idx) + 1];

    u32 d;

    // reduction
    for (d = 128 >> 1; d > 0; d >>= 1)
    {
        groupMemoryBarrier();
        barrier();

        if (local_idx < d)
        {
            u32 l = offset * (2 * local_idx + 1) - 1;
            u32 r = offset * (2 * local_idx + 2) - 1;
            temp[r] += temp[l];
        }
        offset *= 2;
    }

    // clear last element
    if (local_idx == 0)
    {
        if (reduction_group_sum_descriptor != u32_invalid)
        {
            GROUP_SUM_BUFFER[group_id] = temp[128 - 1];
        }
        temp[128 - 1] = 0;
    }

    // downsweep
    for (d = 1; d < 128; d *= 2)
    {
        offset /= 2;

        groupMemoryBarrier();
        barrier();

        if (local_idx < d)
        {
            u32 l = offset * (2 * local_idx + 1) - 1;
            u32 r = offset * (2 * local_idx + 2) - 1;
            u32 tmp = temp[l];
            temp[l] = temp[r];
            temp[r] += tmp;
        }
    }

    groupMemoryBarrier();
    barrier();

    OUTPUT_BUFFER[2 * global_idx]       = temp[2 * local_idx];
    OUTPUT_BUFFER[(2 * global_idx) + 1] = temp[(2 * local_idx) + 1];
}
