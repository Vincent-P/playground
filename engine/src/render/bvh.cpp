#include "render/bvh.h"

#include "render/mesh.h"

#include <cmath>
#include <exo/maths/aabb.h>
#include <exo/algorithms.h>
#include <exo/logger.h>

#include <intrin.h>
#include <limits>

inline float3 extract_float3(__m128 v)
{
    alignas(16) float temp[4];
    _mm_store_ps(temp, v);
    return float3(temp[0], temp[1], temp[2]);
}

AABB calculate_bounds(Vec<TempBVHNode> &temp_nodes, usize prim_start, usize prim_end)
{
    AABB bounds;
    if (prim_start == prim_end)
    {
        bounds.min = float3(0.0f);
        bounds.max = float3(0.0f);
    }
    else
    {
        __m128 bboxMin = _mm_set1_ps(FLT_MAX);
        __m128 bboxMax = _mm_set1_ps(-FLT_MAX);
        for (usize i = prim_start; i < prim_end; ++i)
        {
            __m128 nodeBoundsMin = _mm_loadu_ps(temp_nodes[i].bbox.min.data());
            __m128 nodeBoundsMax = _mm_loadu_ps(temp_nodes[i].bbox.max.data());
            bboxMin              = _mm_min_ps(bboxMin, nodeBoundsMin);
            bboxMax              = _mm_max_ps(bboxMax, nodeBoundsMax);
        }
        bounds.min = extract_float3(bboxMin);
        bounds.max = extract_float3(bboxMax);
    }
    return bounds;
}

static usize temp_bvh_split_median(Vec<TempBVHNode> &temp_nodes, TempBVHNode &node, usize prim_start, usize prim_end)
{
    // get the largest axis
    usize i_max_comp = max_comp(extent(node.bbox));
    // sort triangles nodes, NOTE: iterator+ needs a "difference_type" aka 'long long' for std::vector
    i64 offset_start = static_cast<i64>(prim_start);
    i64 offset_end   = static_cast<i64>(prim_end);
    std::sort(/*std::execution::par_unseq,*/
              temp_nodes.begin() + offset_start,
              temp_nodes.begin() + offset_end,
              [&](const TempBVHNode &a, const TempBVHNode &b) { return a.bbox_center[i_max_comp] < b.bbox_center[i_max_comp]; });
    // split at middle
    float3 split_center = node.bbox_center;
    usize  prim_split   = prim_start;
    for (; prim_split < prim_end; prim_split += 1)
    {
        if (temp_nodes[prim_split].bbox_center[i_max_comp] > split_center[i_max_comp])
        {
            break;
        }
    }
    if (prim_split == prim_end)
    {
        prim_split = prim_end - 1;
    }
    return prim_split;
}

static usize temp_bvh_split_sah(Vec<TempBVHNode> &temp_nodes, TempBVHNode &node, usize prim_start, usize prim_end)
{
    // get the largest axis
    usize i_max_comp = max_comp(extent(node.bbox));

    // sort triangles nodes, NOTE: iterator+ needs a "difference_type" aka 'long long' for std::vector
    i64 offset_start = static_cast<i64>(prim_start);
    i64 offset_end   = static_cast<i64>(prim_end);
    std::sort(/*std::execution::par_unseq, */
              temp_nodes.begin() + offset_start,
              temp_nodes.begin() + offset_end,
              [&](const TempBVHNode &a, const TempBVHNode &b) { return a.bbox_center[i_max_comp] < b.bbox_center[i_max_comp]; });

    struct BucketInfo
    {
        int   count  = 0;
        AABB  bounds = {};
        float cost   = std::numeric_limits<float>::infinity();
    };
    constexpr usize BUCKET_COUNT = 12;
    // cost of a ray-aabb intersection in shader relative to ray-tri intersection (ray-tri 2 times slower)
    constexpr float RAY_BOX_COST = 0.5;

    BucketInfo buckets[BUCKET_COUNT];

    // Place each primitive in a bucket
    for (usize i_prim = prim_start; i_prim < prim_end; i_prim += 1)
    {
        float point_center     = center(temp_nodes[i_prim].bbox)[i_max_comp];
        float point_in_bbox    = point_center - node.bbox.min[i_max_comp];
        float bbox_extent      = extent(node.bbox)[i_max_comp];
        float point_normalized = point_in_bbox / bbox_extent;
        usize i_bucket         = static_cast<usize>(BUCKET_COUNT * point_normalized);

        // A prim center may be coplanar to the node.bbox.max i_max_comp plane
        ASSERT(i_bucket <= BUCKET_COUNT);
        if (i_bucket == BUCKET_COUNT)
        {
            i_bucket = BUCKET_COUNT - 1;
        }
        buckets[i_bucket].count += 1;
        extend(buckets[i_bucket].bounds, temp_nodes[i_prim].bbox);
    }

    // Early-exit if there is only one bucket
    usize non_empty_bucket_count = 0;
    for (usize i_bucket = 0; i_bucket < BUCKET_COUNT; i_bucket += 1)
    {
        if (buckets[i_bucket].count > 0)
        {
            non_empty_bucket_count += 1;
        }
    }

    if (non_empty_bucket_count == 1)
    {
        return prim_start + (prim_end - prim_start) / 2;
    }

    // Compute the cost of splitting after each bucket
    for (usize i_split_bucket = 0; i_split_bucket < BUCKET_COUNT - 1; i_split_bucket += 1)
    {
        AABB left        = {};
        int  left_count  = 0;
        AABB right       = {};
        int  right_count = 0;

        for (usize i_bucket = 0; i_bucket <= i_split_bucket; i_bucket += 1)
        {
            if (buckets[i_bucket].count > 0)
            {
                extend(left, buckets[i_bucket].bounds);
                left_count += buckets[i_bucket].count;
            }
        }

        for (usize i_bucket = i_split_bucket + 1; i_bucket < BUCKET_COUNT; i_bucket += 1)
        {
            if (buckets[i_bucket].count > 0)
            {
                extend(right, buckets[i_bucket].bounds);
                right_count += buckets[i_bucket].count;
            }
        }

        float cost = std::numeric_limits<float>::infinity();
        if (left_count > 0 && right_count > 0)
        {
            auto left_area  = surface(left);
            auto right_area = surface(right);
            auto node_area  = surface(node.bbox);
            cost            = RAY_BOX_COST + (static_cast<float>(left_count) * left_area + static_cast<float>(right_count) * right_area) / node_area;
        }
        buckets[i_split_bucket].cost = cost;
    }

    // Get the cheapest split
    float min_cost     = buckets[0].cost;
    usize i_min_bucket = 0;
    for (usize i_bucket = 1; i_bucket < BUCKET_COUNT - 1; i_bucket += 1)
    {
        if (buckets[i_bucket].cost < min_cost)
        {
            min_cost     = buckets[i_bucket].cost;
            i_min_bucket = i_bucket;
        }
    }

    // Something went wrong
    if (std::isnan(min_cost) || std::isinf(min_cost))
    {
        return prim_start + (prim_end - prim_start) / 2;
    }

    usize prim_split = prim_start;
    for (usize i_bucket = 0; i_bucket < i_min_bucket + 1; i_bucket += 1)
    {
        prim_split += static_cast<usize>(buckets[i_bucket].count);
    }

    ASSERT(prim_start < prim_split && prim_split < prim_end);

    return prim_split;
}

static void create_temp_bvh(BVHScratchMemory &scratch)
{
    auto &prim_start_stack = scratch.prim_start_stack;
    auto &i_node_stack     = scratch.i_node_stack;
    auto &prim_end_stack   = scratch.prim_end_stack;
    auto &temp_nodes       = scratch.temp_nodes;

    auto  prim_count       = temp_nodes.size();

    prim_start_stack.clear();
    i_node_stack.clear();
    prim_end_stack.clear();
    prim_start_stack.reserve(temp_nodes.capacity());
    i_node_stack.reserve(temp_nodes.capacity());
    prim_end_stack.reserve(temp_nodes.capacity());

    i_node_stack.push_back(temp_nodes.size());
    temp_nodes.emplace_back();

    prim_start_stack.push_back(0);
    prim_end_stack.push_back(prim_count);

    while (!prim_start_stack.empty())
    {
        usize i_node = i_node_stack.back();
        i_node_stack.pop_back();
        usize prim_start = prim_start_stack.back();
        prim_start_stack.pop_back();
        usize prim_end = prim_end_stack.back();
        prim_end_stack.pop_back();

        if (prim_start >= prim_end)
        {
            logger::error("BVH: create_temp_bvh should be called with at least one primitive.\n");
            return;
        }

        auto &node = temp_nodes[i_node];

        // Compute internal node's bounding box based on primitive nodes at [prim_start, prim_end[
        node.bbox        = calculate_bounds(temp_nodes, prim_start, prim_end);
        node.bbox_center = center(node.bbox);

#if 1
        usize prim_split = temp_bvh_split_sah(temp_nodes, node, prim_start, prim_end);
#else
        usize prim_split = temp_bvh_split_median(temp_nodes, node, prim_start, prim_end);
#endif

        ASSERT(prim_start < prim_split && prim_split < prim_end);

        // -- Create right child
        if (prim_end - prim_split == 1)
        {
            node.right_child = prim_split;
        }
        else if (prim_end > 1 + prim_split)
        {
            node.right_child = temp_nodes.size();
            temp_nodes.emplace_back();
            i_node_stack.push_back(node.right_child);
            prim_start_stack.push_back(prim_split);
            prim_end_stack.push_back(prim_end);
        }

        // -- Create left child
        // there is only one primitive on the left of the split, create a leaf node
        if (prim_split - prim_start == 1)
        {
            node.left_child = prim_start;
        }
        // more than one primitive, create an internal node
        else if (prim_split > 1 + prim_start)
        {
            node.left_child = temp_nodes.size();
            temp_nodes.emplace_back();
            i_node_stack.push_back(node.left_child);
            prim_start_stack.push_back(prim_start);
            prim_end_stack.push_back(prim_split);
        }
    }
}

// Set the depth_first_index to prefix order, and next_index to skip the subtree
static void bvh_set_temp_order(Vec<TempBVHNode> &temp_nodes, usize &counter, usize i_node, usize i_next)
{
    temp_nodes[i_node].depth_first_index = counter;
    temp_nodes[i_node].next_node_index   = i_next;
    counter += 1;

    if (temp_nodes[i_node].left_child != u64_invalid)
    {
        bvh_set_temp_order(temp_nodes, counter, temp_nodes[i_node].left_child, temp_nodes[i_node].right_child);
    }

    if (temp_nodes[i_node].right_child != u64_invalid)
    {
        bvh_set_temp_order(temp_nodes, counter, temp_nodes[i_node].right_child, i_next);
    }
}

// Create BVH nodes in prefix order, temp_nodes should contain one node per primitive with the correct bounding box
static void create_nodes(BVHScratchMemory &scratch, Vec<BVHNode> &nodes)
{
    auto &temp_nodes = scratch.temp_nodes;
    if (temp_nodes.empty())
    {
        return;
    }

    nodes.clear();

    // emplace root node
    usize root_index = 0;

    if (temp_nodes.size() > 1)
    {
        root_index = temp_nodes.size();
        create_temp_bvh(scratch);
    }

    usize counter = 0;
    bvh_set_temp_order(temp_nodes, counter, root_index, u64_invalid);

    nodes.resize(temp_nodes.size());

    constexpr bool output_graph = false;
    if (output_graph)
    {
        logger::info("graph bvh {{\n");
        logger::info("graph [ordering=\"out\"];\n");
    }

    for (usize i_temp_node = 0; i_temp_node < temp_nodes.size(); i_temp_node += 1)
    {
        auto &temp_node = temp_nodes[i_temp_node];
        auto &node      = nodes[temp_node.depth_first_index];

        node.prim_index = static_cast<u32>(temp_node.prim_index);
        node.bbox_min   = temp_node.bbox.min;
        node.bbox_max   = temp_node.bbox.max;
        node.next_node  = temp_node.next_node_index == u64_invalid ? u32_invalid : static_cast<u32>(temp_nodes[temp_node.next_node_index].depth_first_index);

        if (output_graph)
        {
            logger::info("{} [label=\"{}\"];\n", temp_node.depth_first_index, fmt::format("depth id: {} \\n next: {}\\n face id: {}", temp_node.depth_first_index, node.next_node, node.prim_index));
            if (temp_node.left_child != u64_invalid)
            {
                logger::info("{} -- {};\n", temp_node.depth_first_index, temp_nodes[temp_node.left_child].depth_first_index);
            }
            if (temp_node.right_child != u64_invalid)
            {
                logger::info("{} -- {};\n", temp_node.depth_first_index, temp_nodes[temp_node.right_child].depth_first_index);
            }
        }
    }

    if (output_graph)
    {
        logger::info("}}\n");
    }
}

void create_blas(BVHScratchMemory &scratch, BVH &out, const Vec<u32> &indices, const Vec<float4> &positions)
{
    auto &temp_nodes = scratch.temp_nodes;
    temp_nodes.clear();

    ASSERT(indices.size() % 3 == 0); // not triangles??
    usize primitives_count = indices.size() / 3;
    temp_nodes.reserve(primitives_count * 2);

    auto get_vertex = [&](usize i_index) { return positions[indices[i_index]]; };

    // Compute the bouding box of each triangle
    for (usize i_index = 0; i_index < indices.size(); i_index += 3)
    {
        temp_nodes.emplace_back();
        auto &node      = temp_nodes.back();
        node.prim_index = i_index;
        node.bbox.min   = get_vertex(i_index + 0).xyz();
        node.bbox.max   = node.bbox.min;
        extend(node.bbox, get_vertex(i_index + 1).xyz());
        extend(node.bbox, get_vertex(i_index + 2).xyz());
        node.bbox_center = center(node.bbox);
    }

    ASSERT(temp_nodes.size() == primitives_count);

    out.nodes.clear();
    create_nodes(scratch, out.nodes);
}

void create_tlas(BVHScratchMemory &scratch, BVH &out, const Vec<BVHNode> &blas_roots, const Vec<float4x4> &blas_transforms, const Vec<u32> &blas_indices)
{
    ASSERT(blas_roots.size() == blas_transforms.size());
    ASSERT(blas_roots.size() == blas_indices.size());

    auto &temp_nodes = scratch.temp_nodes;
    temp_nodes.clear();

    usize primitives_count = blas_roots.size();
    out.nodes.clear();
    out.nodes.reserve(primitives_count * 2);
    temp_nodes.reserve(primitives_count * 2);

    // Compute the bouding box of each primitive
    for (usize i_blas = 0; i_blas < blas_roots.size(); i_blas += 1)
    {
        const auto &blas      = blas_roots[i_blas];
        const auto &transform = blas_transforms[i_blas];

        temp_nodes.emplace_back();
        auto &node      = temp_nodes.back();
        node.prim_index = blas_indices[i_blas];

        float3 corners[] = {
            {blas.bbox_min.x, blas.bbox_min.y, blas.bbox_min.z},
            {blas.bbox_min.x, blas.bbox_min.y, blas.bbox_max.z},
            {blas.bbox_min.x, blas.bbox_max.y, blas.bbox_min.z},
            {blas.bbox_min.x, blas.bbox_max.y, blas.bbox_max.z},
            {blas.bbox_max.x, blas.bbox_min.y, blas.bbox_min.z},
            {blas.bbox_max.x, blas.bbox_min.y, blas.bbox_max.z},
            {blas.bbox_max.x, blas.bbox_max.y, blas.bbox_min.z},
            {blas.bbox_max.x, blas.bbox_max.y, blas.bbox_max.z},
        };

        for (usize i_corner = 0; i_corner < ARRAY_SIZE(corners); i_corner += 1)
        {
            corners[i_corner] = (transform * float4(corners[i_corner], 1.0)).xyz();
        }

        node.bbox.min = corners[0];
        node.bbox.max = corners[0];
        for (usize i_corner = 1; i_corner < ARRAY_SIZE(corners); i_corner += 1)
        {
            extend(node.bbox, corners[i_corner]);
        }

        node.bbox_center = center(node.bbox);
    }

    ASSERT(temp_nodes.size() == primitives_count);

    create_nodes(scratch, out.nodes);
}
