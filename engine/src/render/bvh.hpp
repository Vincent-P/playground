#pragma once

#include "base/types.hpp"
#include "base/vector.hpp"
#include "base/pool.hpp"

struct Vertex;
struct Mesh;
struct Material;
struct RenderMeshData;
struct GpuPool;

struct Face
{
    u32 first_index = u32_invalid;
    u32 mesh_id     = u32_invalid;
    u32 material_id = u32_invalid;
    u32 pad1;
};

struct PACKED BVHNode
{
    float3 bbox_min;
    u32    face_index = u32_invalid;
    float3 bbox_max;
    u32    next_node = u32_invalid;
};

struct BVH
{
    Vec<Face> faces;
    Vec<BVHNode> nodes;
};

BVH create_bvh(const Vec<u32> &render_meshes_indices, const GpuPool &render_meshes_data, const GpuPool &vertices, const GpuPool &indices, const Vec<Material> &materials);
