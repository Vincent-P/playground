#pragma once
#include <exo/prelude.h>
#include <exo/collections/vector.h>

struct PACKED BVHNode
{
    float3 bbox_min   = {0.0f};
    u32    prim_index = u32_invalid;
    float3 bbox_max   = {0.0f};
    u32    next_node  = u32_invalid;
};

struct BVH
{
    Vec<BVHNode> nodes;
};

BVH create_blas(const Vec<u32> &indices, const Vec<float4> &positions);
BVH create_tlas(const Vec<BVHNode> &blas_roots, const Vec<float4x4> &blas_transforms, const Vec<u32> &blas_indices);
