#include "render/bvh.h"

#include "render/mesh.h"

#include <exo/aabb.h>
#include <exo/algorithms.h>
#include <exo/logger.h>

#include <intrin.h>

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

static void create_temp_bvh(Vec<TempBVHNode> &temp_nodes, usize i_node, usize prim_start, usize prim_end)
{
    Vec<usize> i_node_stack(temp_nodes.capacity());
    Vec<usize> prim_start_stack(temp_nodes.capacity());
    Vec<usize> prim_end_stack(temp_nodes.capacity());

    i_node_stack.push_back(i_node);
    prim_start_stack.push_back(prim_start);
    prim_end_stack.push_back(prim_end);

    while (!i_node_stack.empty())
    {
        usize i_node = i_node_stack.back(); i_node_stack.pop_back();
        usize prim_start = prim_start_stack.back(); prim_start_stack.pop_back();
        usize prim_end = prim_end_stack.back(); prim_end_stack.pop_back();

        auto &node = temp_nodes[i_node];

        if (prim_end <= prim_start) {
            // logger::error("BVH: create_temp_bvh should be called with at least one primitive.\n");
            return;
        }

        // Compute internal node's bounding box based on primitive nodes at [prim_start, prim_end[
        node.bbox = calculate_bounds(temp_nodes, prim_start, prim_end);
        node.bbox_center = center(node.bbox);

        // -- Median splitting
        // get the largest axis
        usize i_max_comp = max_comp(extent(node.bbox));
        // sort triangles nodes, NOTE: iterator+ needs a "difference_type" aka 'long long' for std::vector
        i64 offset_start = static_cast<i64>(prim_start);
        i64 offset_end = static_cast<i64>(prim_end);
        std::sort(std::execution::par_unseq, temp_nodes.begin() + offset_start, temp_nodes.begin() + offset_end, [&](const TempBVHNode &a, const TempBVHNode &b) { return a.bbox_center[i_max_comp] < b.bbox_center[i_max_comp]; });
        // split at middle
        float3 split_center = node.bbox_center;
        usize prim_split     = prim_start;
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

        if (!(prim_start <= prim_split && prim_split < prim_end))
        {
            DEBUG_BREAK();
        }

        // -- Create right child
        if (prim_end - prim_split == 1)
        {
            temp_nodes[i_node].right_child = prim_split;
        }
        else if (prim_end > 1 + prim_split)
        {
            temp_nodes[i_node].right_child = temp_nodes.size();
            temp_nodes.emplace_back();

            // create_bvh_rec(temp_nodes, temp_nodes[i_node].right_child, prim_split, prim_end);
            i_node_stack.push_back(temp_nodes[i_node].right_child);
            prim_start_stack.push_back(prim_split);
            prim_end_stack.push_back(prim_end);
        }

        // -- Create left child
        // there is only one primitive on the left of the split, create a leaf node
        if (prim_split - prim_start == 1)
        {
            temp_nodes[i_node].left_child = prim_start;
        }
        // more than one primitive, create an internal node
        else if (prim_split > 1 + prim_start)
        {
            temp_nodes[i_node].left_child = temp_nodes.size();
            temp_nodes.emplace_back();
            // create_bvh_rec(temp_nodes, temp_nodes[i_node].left_child, prim_start, prim_split);
            i_node_stack.push_back(temp_nodes[i_node].left_child);
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
static Vec<BVHNode> create_nodes(Vec<TempBVHNode> &&temp_nodes)
{
    if (temp_nodes.empty())
    {
        return {};
    }

    Vec<BVHNode> nodes;

    auto primitives_count = temp_nodes.size();
    auto root_index       = temp_nodes.size();

    // emplace root node
    temp_nodes.emplace_back();
    create_temp_bvh(temp_nodes, root_index, 0, primitives_count);

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

    return nodes;
}

BVH create_blas(const Vec<u32> &indices, const Vec<float4> &positions)
{
    BVH bvh;
    Vec<TempBVHNode> temp_nodes;

    assert(indices.size() % 3 == 0); // not triangles??
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

    assert(temp_nodes.size() == primitives_count);

    bvh.nodes = create_nodes(std::move(temp_nodes));

    return bvh;
}

BVH create_tlas(const Vec<BVHNode> &blas_roots, const Vec<float4x4> &blas_transforms, const Vec<u32> &blas_indices)
{
    assert(blas_roots.size() == blas_transforms.size());
    assert(blas_roots.size() == blas_indices.size());

    BVH bvh;
    Vec<TempBVHNode> temp_nodes;

    usize primitives_count = blas_roots.size();
    bvh.nodes.reserve(primitives_count * 2);
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

        bvh.nodes.emplace_back();
    }

    assert(temp_nodes.size() == primitives_count);

    bvh.nodes = create_nodes(std::move(temp_nodes));

    return bvh;
}
