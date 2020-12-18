#pragma shader_stage(compute)

#include "types.h"
#include "globals.h"
#include "csm.h"

layout (set = 1, binding = 0) uniform sampler2D reduction_input;

layout(set = 1, binding = 1) buffer GltfVertexBuffer {
    float4 depth_slices;
    CascadeMatrix matrices[];
};

// TODO: set 4 threads to compute cascades in parallel
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main()
{
    float2 max_min_depth = texelFetch(reduction_input, int2(0, 0), LOD0).rg;
}
