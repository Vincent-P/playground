#pragma once
#include <exo/prelude.h>
#include <exo/collections/vector.h>
#include <exo/maths/aabb.h>

struct TempBVHNode
{
    // internal nodes
    AABB   bbox        = {};
    float3 bbox_center = {0.0f};
    usize  left_child  = u64_invalid;
    usize  right_child = u64_invalid;

    // traversal order
    usize depth_first_index = u64_invalid;
    usize next_node_index   = u64_invalid;

    // geometry indices
    usize prim_index = u64_invalid;
};

PACKED(struct BVHNode
{
    float3 bbox_min   = {0.0f};
    u32    prim_index = u32_invalid;
    float3 bbox_max   = {0.0f};
    u32    next_node  = u32_invalid;
};)

struct BVHScratchMemory
{
    Vec<TempBVHNode> temp_nodes;
    Vec<usize> prim_start_stack;
    Vec<usize> i_node_stack;
    Vec<usize> prim_end_stack;
    Vec<BVHNode> nodes;
};

struct BVH
{
    Vec<BVHNode> nodes;
};

void create_blas(BVHScratchMemory &scratch, BVH &out, const Vec<u32> &indices, const Vec<float4> &positions);
void create_tlas(BVHScratchMemory &scratch, BVH &out, const Vec<BVHNode> &blas_roots, const Vec<float4x4> &blas_transforms, const Vec<u32> &blas_indices);
