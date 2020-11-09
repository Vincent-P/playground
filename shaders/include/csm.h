#ifndef CSM
#define CSM
#include "types.h"

struct CascadeMatrix
{
    float4x4 view;
    float4x4 proj;
};

const float3 cascade_colors[] = {
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0),
    float3(0.0, 1.0, 1.0),
    };

#endif
