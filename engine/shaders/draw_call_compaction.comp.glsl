#pragma shader_stage(compute)

#include "types.h"
#include "constants.h"
#include "globals.h"
#define PBR_NO_NORMALS
#include "pbr.h"
#include "maths.h"

struct DrawIndirectCommand
{
    u32 index_count;
    u32 instance_count;
    u32 first_index;
    i32 vertex_offset;
    u32 first_instance;
};

layout(set = 1, binding = 0) buffer readonly DrawCommandsInput {
    u32 draw_count;
    DrawIndirectCommand commands[];
} in_draws;

layout(set = 1, binding = 1) buffer readonly DrawDatasInput {
    DrawData in_datas[];
};

layout (set = 1, binding = 2) buffer readonly Visibility {
    u32 visibility[];
};

layout(set = 1, binding = 3) buffer writeonly DrawCommandsOutput {
    u32 draw_count;
    DrawIndirectCommand commands[];
} out_draws;


layout(set = 1, binding = 4) buffer writeonly DrawDatasOutput {
    DrawData out_datas[];
};


bool inside_plane(float3 point, float3 plane_normal)
{
    return dot(point, plane_normal) > 0;
}

shared u32 scan[2048];


// scan algorithm from https://developer.nvidia.com/gpugems/gpugems3/part-vi-gpu-computing/chapter-39-parallel-prefix-sum-scan-cuda
layout(local_size_x = 1024, local_size_y = 1, local_size_z = 1) in;
void main()
{
    int local_idx = int(gl_LocalInvocationIndex);

    int offset = 1;
    int n = 2048;

    // load input into shared memory
    scan[2*local_idx]   = visibility[2*local_idx];
    scan[2*local_idx+1] = visibility[2*local_idx+1];

    // build sum in place up the tree
    for (int d = n>>1; d > 0; d >>= 1)
    {
        groupMemoryBarrier();
        barrier();

        if (local_idx < d)
        {
            int ai = offset*(2*local_idx+1)-1;
            int bi = offset*(2*local_idx+2)-1;
            scan[bi] += scan[ai];
        }
        offset *= 2;
    }

    // clear the last element
    if (local_idx == 0)
    {
        out_draws.draw_count = scan[n-1];
        scan[n - 1] = 0;
    }

    // traverse down tree & build scan
    for (int d = 1; d < n; d *= 2)
    {
        offset >>= 1;

        groupMemoryBarrier();
        barrier();

        if (local_idx < d)
        {
            int ai = offset*(2*local_idx+1)-1;
            int bi = offset*(2*local_idx+2)-1;

            uint t = scan[ai];
            scan[ai] = scan[bi];
            scan[bi] += t;
        }
    }

    groupMemoryBarrier();
    barrier();

    if (visibility[2*local_idx] != 0)
    {
        out_draws.commands[scan[2*local_idx]] = in_draws.commands[2*local_idx];
        out_datas[scan[2*local_idx]] = in_datas[2*local_idx];
    }
    if (visibility[2*local_idx+1] != 0)
    {
        out_draws.commands[scan[2*local_idx+1]] = in_draws.commands[2*local_idx+1];
        out_datas[scan[2*local_idx+1]] = in_datas[2*local_idx+1];
    }
}
