#pragma once
#include "base/types.hpp"
#include "base/option.hpp"
#include "base/vector.hpp"

#include <string>
#include <string_view>
#include <limits>
#include <filesystem>

namespace gltf
{
enum class RenderingMode : u32
{
    Points        = 0,
    Lines         = 1,
    LineLoop      = 2,
    LineStrip     = 3,
    Triangles     = 4,
    TriangleStrip = 5,
    TriangleFan   = 6
};

// https://gist.github.com/szimek/763999
enum class ComponentType : u32
{
    Byte          = 5120,
    UnsignedByte  = 5121,
    Short         = 5122,
    UnsignedShort = 5123,
    Int           = 5124,
    UnsignedInt   = 5125,
    Float         = 5126,
    Double        = 5130
};

enum class Filter : u32
{
    Nearest              = 9728,
    Linear               = 9729,
    NearestMipMapNearest = 9984,
    LinearMipMapNearest  = 9985,
    NearestMiMapLinear   = 9986,
    LinearMipMapLinear   = 9987
};

enum class Wrap
{
    Repeat         = 10497,
    ClampToEdge    = 33071,
    MirroredRepeat = 33648
};

struct Material
{
    float4 base_color_factor       = float4(1.0f);
    float4 emissive_factor         = float4(0.0f);
    float metallic_factor          = 1.0f;
    float roughness_factor         = 1.0f;
    u32 base_color_texture         = u32_invalid;
    u32 normal_texture             = u32_invalid;
    u32 metallic_roughness_texture = u32_invalid;
    float3 padding00;
} PACKED;

struct Image
{
    bool srgb;
    Vec<u8> data;
};

struct Sampler
{
    Filter mag_filter = Filter::Linear;
    Filter min_filter = Filter::Linear;
    Wrap wrap_s       = Wrap::Repeat;
    Wrap wrap_t       = Wrap::Repeat;
};

struct Texture
{
    usize image;
    usize sampler;
};

struct Buffer
{
    usize byte_length;
    Vec<u8> data;
};

enum class AccessorType
{
    Scalar,
    Vec3,
    Vec4,
    Mat4
};

inline Option<AccessorType> accessor_type_from_str(const std::string &string)
{
    if (string == "SCALAR")
    {
        return AccessorType::Scalar;
    }
    if (string == "VEC3")
    {
        return AccessorType::Vec3;
    }
    if (string == "VEC4")
    {
        return AccessorType::Vec4;
    }
    if (string == "MAT4")
    {
        return AccessorType::Mat4;
    }

    return std::nullopt;
}

struct PACKED Primitive
{
    u32 material;
    u32 first_index;
    u32 first_vertex;
    u32 index_count;

    float3 aab_min = float3(std::numeric_limits<float>::infinity());
    RenderingMode mode = RenderingMode::Triangles;

    float3 aab_max = float3(-std::numeric_limits<float>::infinity());
    u32 pad00;
};

struct Mesh
{
    std::string name;
    Vec<u32> primitives;
};

struct Node
{
    Option<usize> mesh;

    bool dirty{true};

    float3 translation{};
    float3 scale{1.0f};
    float4 rotation{};
    float4x4 transform{1.0f};

    Vec<u32> children;
};

struct PACKED Vertex
{
    float3 position;
    float pad00;
    float3 normal;
    float pad01;
    float2 uv0;
    float2 uv1;
    float4 color0 = float4(1.0f);
    float4 joint0;
    float4 weight0;
};

struct Model
{
    std::filesystem::path path;
    Vec<usize> scene;
    Vec<Node> nodes;
    Vec<Mesh> meshes;
    Vec<Primitive> primitives;
    Vec<Buffer> buffers;
    Vec<Material> materials;
    Vec<Texture> textures;
    Vec<Sampler> samplers;
    Vec<Image> images;

    Vec<Vertex>vertices;
    Vec<usize> indices;

    Vec<usize> nodes_preorder;
    Vec<float4x4> cached_transforms;
};

Model load_model(std::filesystem::path path);
} // namespace my_app
