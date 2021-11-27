#pragma once
#include <exo/maths/vectors.h>
#include <exo/collections/vector.h>
#include <exo/collections/handle.h>

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

// Dependencies: Material
struct Mesh : Asset
{
    Vec<u32> indices;
    Vec<float4> positions;
    Vec<float2> uvs;
    Vec<SubMesh> submeshes;

    const char *type_name() const final { return "Mesh"; }
    void from_flatbuffer(const void *data, usize len) final;
    void to_flatbuffer(flatbuffers::FlatBufferBuilder &builder, u32 &o_offset, u32 &o_size) const final;

    // doesn't check the name
    bool is_similar(const Mesh &other) const { return indices == other.indices && positions == other.positions && submeshes == other.submeshes; }
    bool operator==(const Mesh &other) const = default;
};
