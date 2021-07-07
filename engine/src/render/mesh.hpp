#pragma once
#include "base/types.hpp"
#include "base/vector.hpp"

#include <string>

struct PACKED Vertex
{
    float3 position;
    float pad00;
    float3 normal;
    float pad01;
    float2 uv0;
    float2 uv1;
    float4 color0 = float4(1.0f);

    bool operator==(const Vertex &other) const = default;
};

struct SubMesh
{
    u32 first_index;
    u32 first_vertex;
    u32 index_count;
    u32 vertex_count;
};

struct Mesh
{
    std::string name;
    Vec<u32> indices;
    Vec<float3> positions;
    Vec<SubMesh> submeshes;
    Vec<float4x4> submeshes_transforms;

    bool operator==(const Mesh &other) const = default;
};
