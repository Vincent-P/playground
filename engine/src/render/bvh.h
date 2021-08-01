#pragma once
#include <exo/types.h>
#include <exo/collections/vector.h>

struct PACKED BVHNode
{
    float3 bbox_min;
    u32    prim_index = u32_invalid;
    float3 bbox_max;
    u32    next_node = u32_invalid;
};

struct BVH
{
    Vec<BVHNode> nodes;
};

BVH create_blas(const Vec<u32> &indices, const Vec<float4> &positions);
BVH create_tlas(const Vec<BVHNode> &blas_roots, const Vec<float4x4> &blas_transforms, const Vec<u32> &blas_indices);
