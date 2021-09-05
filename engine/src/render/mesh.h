#pragma once
#include <exo/types.h>
#include <exo/collections/vector.h>

#include <string>

struct SubMesh
{
    u32 first_index = 0;
    u32 first_vertex = 0;
    u32 index_count = 0;
    u32 i_material = 0;

    bool operator==(const SubMesh &other) const = default;
};

struct Mesh
{
    std::string name;
    Vec<u32> indices;
    Vec<float4> positions;
    Vec<float2> uvs;
    Vec<SubMesh> submeshes;

    // doesn't check the name
    bool is_similar(const Mesh &other) const { return indices == other.indices && positions == other.positions && submeshes == other.submeshes; }
    bool operator==(const Mesh &other) const = default;
};
