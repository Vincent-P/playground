#pragma once
#include <exo/types.h>
#include <exo/collections/vector.h>

#include <string>

struct SubMesh
{
    u32 first_index;
    u32 first_vertex;
    u32 index_count;
    u32 vertex_count;
    u32 i_material;

    bool operator==(const SubMesh &other) const = default;
};

struct Mesh
{
    std::string name;
    Vec<u32> indices;
    Vec<float4> positions;
    Vec<SubMesh> submeshes;

    // doesn't check the name
    bool is_similar(const Mesh &other) const { return indices == other.indices && positions == other.positions && submeshes == other.submeshes; }
    bool operator==(const Mesh &other) const = default;
};
