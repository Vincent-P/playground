#pragma shader_stage(compute)

#include "types.h"

layout (set = 1, binding = 0) uniform sampler2D depth_buffer;
layout (set = 1, binding = 1, rg32f) uniform image2D reduction_output;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

shared float2 reduction_shared[16*16];

void main()
{
    uint local_idx = gl_LocalInvocationIndex;
    uint3 global_idx = gl_GlobalInvocationID;
    uint3 group_idx = gl_WorkGroupID;

    int2 depth_buffer_size = textureSize(depth_buffer, LOD0);

    const float2 IDENTITY_REDUCTION = float2(0.0, 1.0);
    // each thread will sample the depth buffer
    if(global_idx.x < depth_buffer_size.x && global_idx.y < depth_buffer_size.y)
    {
        float depth = texelFetch(depth_buffer, int2(global_idx.xy), LOD0).r;

        reduction_shared[local_idx] = depth != 0.0 ? float2(depth) : IDENTITY_REDUCTION;
    }
    else
    {
        reduction_shared[local_idx] = IDENTITY_REDUCTION;
    }

    groupMemoryBarrier();
    barrier();

    // reduce min/max
    for (uint sample_idx = (256 >> 1); sample_idx > 0; sample_idx >>= 1)
    {
        if(local_idx < sample_idx)
        {
            reduction_shared[local_idx].x = max(reduction_shared[local_idx].x, reduction_shared[local_idx + sample_idx].x);
            reduction_shared[local_idx].y = min(reduction_shared[local_idx].y, reduction_shared[local_idx + sample_idx].y);
        }

        groupMemoryBarrier();
        barrier();
    }

    // output the value
    if (local_idx == 0)
    {
        imageStore(reduction_output, int2(group_idx.xy), float4(reduction_shared[0], 0.0, 0.0));
    }
}
