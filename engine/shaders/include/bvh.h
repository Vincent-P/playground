#ifndef BVH_HEADER
#define BVH_HEADER

#include "types.h"

struct Face
{
    u32 first_index;
    u32 mesh_id;
    u32 material_id;
    u32 pad1;
};

struct BVHNode
{
    float3 bbox_min;
    u32    face_index;
    float3 bbox_max;
    u32    next_node;
};

#endif
