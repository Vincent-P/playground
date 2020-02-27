#pragma once
#include "types.hpp"
#include <vector>
#include <optional>
#include "renderer/hl_api.hpp"

namespace my_app
{
enum class RenderingMode : u8
{
    Points = 0,
    Lines = 1,
    LineLoop = 2,
    LineStrip = 3,
    Triangles = 4,
    TriangleStrip = 5,
    TriangleFan = 6
};

struct Material
{
    float3      base_color_factor;
    float       metallic_factor;
    const char *name;
};

struct Buffer
{
    u32             byte_length;
    std::vector<u8> data;
    vulkan::BufferH buffer_h;
};

enum class AccessorType
{
    Scalar,
    Vec3,
    Vec4,
    Mat4
};

inline std::optional<AccessorType> accessor_type_from_str(const std::string& string)
{
    if (string == "SCALAR") {
        return AccessorType::Scalar;
    }
    else if (string == "VEC3") {
        return AccessorType::Vec3;
    }
    else if (string == "VEC4") {
        return AccessorType::Vec4;
    }
    else if (string == "MAT4") {
        return AccessorType::Mat4;
    }

    return std::nullopt;
}

// https://gist.github.com/szimek/763999
enum class ComponentType
{
    Byte = 5120,
    UnsignedByte = 5121,
    Short = 5122,
    UnsignedShort = 5123,
    Int = 5124,
    UnsignedInt = 5125,
    Float = 5126
};

struct Primitive
{
    RenderingMode mode;
    u32           material;

    u32 first_index;
    u32 first_vertex;
    u32 index_count;
};

struct Mesh
{
    const char *           name;
    std::vector<Primitive> primitives;
};

struct GltfVertex
{
    float3 position;
    float3 normal;
    float2 uv0;
    float2 uv1;
    float4 joint0;
    float4 weight0;
};

struct Model
{
    std::vector<Mesh>       meshes;
    std::vector<Primitive>  primitives;
    std::vector<Material>   materials;
    std::vector<Buffer>     buffers;

    std::vector<GltfVertex> vertices;
    std::vector<u16>        indices;

    vulkan::BufferH vertex_buffer;
    vulkan::BufferH index_buffer;
};

Model load_model(const char *path);
} // namespace my_app
