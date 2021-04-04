#include "render/bvh.hpp"

#include "base/logger.hpp"
#include "base/intrinsics.hpp"
#include "base/algorithms.hpp"
#include "render/material.hpp"
#include "render/renderer.hpp"
#include "gltf.hpp"

static float3 box_center(float3 bbox_min, float3 bbox_max)
{
    return (bbox_min + bbox_max) * 0.5f;
}

static float3 box_extent(float3 bbox_min, float3 bbox_max)
{
    return (bbox_max - bbox_min);
}

static void box_extend(float3 &bbox_min, float3 &bbox_max, float3 new_point)
{
    for (uint i_comp = 0; i_comp < 3; i_comp += 1)
    {
        if (new_point.raw[i_comp] < bbox_min.raw[i_comp]) {
            bbox_min.raw[i_comp] = new_point.raw[i_comp];
        }
        if (new_point.raw[i_comp] > bbox_max.raw[i_comp]) {
            bbox_max.raw[i_comp] = new_point.raw[i_comp];
        }
    }
}

static void box_union(float3 &bbox_min, float3 &bbox_max, float3 other_min, float3 other_max)
{
    box_extend(bbox_min, bbox_max, other_min);
    box_extend(bbox_min, bbox_max, other_max);
}

struct TempBVHNode
{
    // internal nodes
    float3 bbox_min;
    float3 bbox_max;
    float3 bbox_center;
    u32 left_child = u32_invalid;
    u32 right_child = u32_invalid;

    // traversal order
    u32 depth_first_index = u32_invalid;
    u32 next_node_index = u32_invalid;

    // geometry indices
    u32 face_index = u32_invalid;
};

// creates internal nodes
static void create_bvh_rec(Vec<TempBVHNode> &temp_nodes, usize i_node, usize tri_start, usize tri_end)
{
    auto &node = temp_nodes[i_node];

    auto tri_count = tri_end - tri_start;

    if (tri_count <= 1 || tri_end <= tri_start)
    {
        DEBUG_BREAK();
    }

    // Compute internal node's bounding box based on primitive nodes at [tri_start, tri_end[
    node.bbox_min = temp_nodes[tri_start].bbox_min;
    node.bbox_max = temp_nodes[tri_start].bbox_max;
    for (usize i_tri = tri_start + 1; i_tri < tri_end; i_tri += 1)
    {
        box_union(node.bbox_min, node.bbox_max, temp_nodes[i_tri].bbox_min, temp_nodes[i_tri].bbox_max);
    }
    node.bbox_center = box_center(node.bbox_min, node.bbox_max);

    // get the largest axis
    uint max_comp = box_extent(node.bbox_min, node.bbox_max).max_comp();

    // sort triangles nodes
    std::sort(temp_nodes.begin() + tri_start, temp_nodes.begin() + tri_end, [&](const TempBVHNode &a , const TempBVHNode &b) {
        return a.bbox_center.raw[max_comp] < b.bbox_center.raw[max_comp];
    });

    // split at middle
    float3 split_center = node.bbox_center;
    usize tri_split = tri_start;
    for (; tri_split < tri_end; tri_split += 1)
    {
        if (temp_nodes[tri_split].bbox_center.raw[max_comp] > split_center.raw[max_comp])
        {
            break;
        }
    }
    if (tri_split == tri_end) {
        tri_split = tri_end - 1;
    }

    if (!(tri_start < tri_split && tri_split < tri_end))
    {
        DEBUG_BREAK();
    }

    // Create left child
    // it's a leaf, point to triangle node
    if (tri_split - tri_start == 1)
    {
        temp_nodes[i_node].left_child = tri_start;
    }
    // it's an internal node, needs to push_back
    else
    {
        temp_nodes[i_node].left_child = temp_nodes.size();
        temp_nodes.emplace_back();
        create_bvh_rec(temp_nodes, temp_nodes[i_node].left_child, tri_start, tri_split);
    }

    // Create right child
    if (tri_end - tri_split == 1)
    {
        temp_nodes[i_node].right_child = tri_split;
    }
    else
    {
        temp_nodes[i_node].right_child = temp_nodes.size();
        temp_nodes.emplace_back();
        create_bvh_rec(temp_nodes, temp_nodes[i_node].right_child, tri_split, tri_end);
    }
}

static void bvh_set_temp_order(Vec<TempBVHNode> &temp_nodes, usize &counter, usize i_node, usize i_next)
{
    temp_nodes[i_node].depth_first_index = counter;
    temp_nodes[i_node].next_node_index = i_next;
    counter += 1;

    if (temp_nodes[i_node].left_child != u32_invalid) {
        bvh_set_temp_order(temp_nodes, counter, temp_nodes[i_node].left_child, temp_nodes[i_node].right_child);
    }

    if (temp_nodes[i_node].right_child != u32_invalid) {
        bvh_set_temp_order(temp_nodes, counter, temp_nodes[i_node].right_child, i_next);
    }
}

BVH create_bvh(const Vec<u32> &render_meshes_indices, const GpuPool &render_meshes_data, const Vec<Vertex> &vertices, const Vec<u32> &indices, const Pool<Mesh> &meshes, const Vec<Material> &materials)
{
    assert(indices.size() % 3 == 0); // not triangles??
    usize triangles_count = indices.size() / 3;

    BVH bvh;
    Vec<TempBVHNode> temp_nodes;

    bvh.faces.reserve(triangles_count);
    bvh.nodes.reserve(triangles_count * 2);
    temp_nodes.reserve(triangles_count * 2);

    // Process every triangles
    for (auto i_render_mesh : render_meshes_indices)
    {
        const auto &render_mesh = render_meshes_data.get<RenderMeshData>(i_render_mesh);
        const auto &mesh = *meshes.get(render_mesh.mesh_handle);

        for (usize i_prim_index = 0; i_prim_index < mesh.index_count; i_prim_index += 3)
        {
            u32 i_index = mesh.index_offset + i_prim_index;

            temp_nodes.emplace_back();
            auto &node = temp_nodes.back();
            node.face_index = bvh.faces.size();

            bvh.faces.emplace_back();
            auto &face = bvh.faces.back();

            face.mesh_id = i_render_mesh;
            assert(i_index % 3 == 0);
            face.first_index = i_index;

            bvh.nodes.emplace_back();

            node.bbox_min = (render_mesh.transform * float4(vertices[indices[i_index + 0]].position, 1.0)).xyz();
            node.bbox_max = node.bbox_min;

            box_extend(node.bbox_min, node.bbox_max, (render_mesh.transform * float4(vertices[indices[i_index + 1]].position, 1.0)).xyz());
            box_extend(node.bbox_min, node.bbox_max, (render_mesh.transform * float4(vertices[indices[i_index + 2]].position, 1.0)).xyz());

            node.bbox_center = box_center(node.bbox_min, node.bbox_max);
        }
    }

    assert(bvh.faces.size()  == triangles_count);
    assert(temp_nodes.size() == triangles_count);

    u32 root_index = temp_nodes.size();
    temp_nodes.emplace_back();
    create_bvh_rec(temp_nodes, root_index, 0, triangles_count);

    usize counter = 0;
    bvh_set_temp_order(temp_nodes, counter, root_index, u32_invalid);

    bvh.nodes.resize(temp_nodes.size());

    constexpr bool output_graph = false;
    if (output_graph)
    {
        logger::info("graph bvh {{\n");
        logger::info("graph [ordering=\"out\"];\n");
    }

    for (usize i_temp_node = 0; i_temp_node < temp_nodes.size(); i_temp_node += 1)
    {
        auto &temp_node = temp_nodes[i_temp_node];
        auto &node = bvh.nodes[temp_node.depth_first_index];

        node.face_index = temp_node.face_index;
        node.bbox_min = temp_node.bbox_min;
        node.bbox_max = temp_node.bbox_max;
        node.next_node = temp_node.next_node_index == u32_invalid ? u32_invalid : temp_nodes[temp_node.next_node_index].depth_first_index;

        if (output_graph)
        {
            logger::info("{} [label=\"{}\"];\n", temp_node.depth_first_index, fmt::format("depth id: {} \\n next: {}\\n face id: {}", temp_node.depth_first_index, node.next_node, node.face_index));
            if (temp_node.left_child != u32_invalid)
            {
                logger::info("{} -- {};\n", temp_node.depth_first_index, temp_nodes[temp_node.left_child].depth_first_index);
            }
            if (temp_node.right_child != u32_invalid)
            {
                logger::info("{} -- {};\n", temp_node.depth_first_index, temp_nodes[temp_node.right_child].depth_first_index);
            }
        }
    }

    if (output_graph)
    {
        logger::info("}}\n");
    }

    UNUSED(materials);

    return bvh;
}
