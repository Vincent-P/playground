#pragma once
#include <exo/maths/vectors.h>
#include <exo/collections/vector.h>
#include <exo/collections/handle.h>
#include <exo/serializer.h>

#include "assets/asset.h"

struct Material;

struct SubMesh
{
    u32         first_index  = 0;
    u32         first_vertex = 0;
    u32         index_count  = 0;
    cross::UUID material     = {};

    bool operator==(const SubMesh &other) const = default;
};

template<>
inline void Serializer::serialize<SubMesh>(SubMesh &data)
{
    serialize(data.first_index);
    serialize(data.first_vertex);
    serialize(data.index_count);
    serialize(data.material);
}

// Dependencies: Material
struct Mesh : Asset
{
    const char *type_name() const final { return "Mesh"; }
    void serialize(Serializer& serializer) final;
    void display_ui() final {}

    // doesn't check the name
    bool is_similar(const Mesh &other) const { return indices == other.indices && positions == other.positions && submeshes == other.submeshes; }
    bool operator==(const Mesh &other) const = default;

    // --
    Vec<u32> indices;
    Vec<float4> positions;
    Vec<float2> uvs;
    Vec<SubMesh> submeshes;
};

template<>
void Serializer::serialize<Mesh>(Mesh &data);
