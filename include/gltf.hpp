#pragma once
#include "renderer/hl_api.hpp"
#include "base/types.hpp"
#include <optional>
#include <string>
#include <vector>

namespace my_app
{
enum class RenderingMode : u8
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
enum class ComponentType
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

enum class Filter
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
    float4 base_color_factor = float4(1.0f);
    float metallic_factor = 1.0f;
    float roughness_factor = 1.0f;
    std::optional<u32> base_color_texture = {};
    std::optional<u32> normal_texture = {};
    std::optional<u32> metallic_roughness_texture = {};
};

struct Image
{
    bool srgb;
    std::vector<u8> data;
};

struct Sampler
{
    Filter mag_filter;
    Filter min_filter;
    Wrap wrap_s;
    Wrap wrap_t;
};

struct Texture
{
    u32 image;
    u32 sampler;
};

struct Buffer
{
    u32 byte_length;
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

inline std::optional<AccessorType> accessor_type_from_str(const std::string &string)
{
    if (string == "SCALAR") {
	return AccessorType::Scalar;
    }
    if (string == "VEC3") {
	return AccessorType::Vec3;
    }
    if (string == "VEC4") {
	return AccessorType::Vec4;
    }
    if (string == "MAT4") {
	return AccessorType::Mat4;
    }

    return std::nullopt;
}

struct Primitive
{
    RenderingMode mode;
    u32 material;

    u32 first_index;
    u32 first_vertex;
    u32 index_count;
};

struct Mesh
{
    const char *name;
    std::vector<Primitive> primitives;
};

struct Node
{
    usize mesh;

    bool dirty{true};

    float3 translation{};
    float3 scale{1.0f};
    float4 rotation{};

    std::vector<u32> children;
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
    std::vector<usize> scene;
    std::vector<Node> nodes;
    std::vector<Mesh> meshes;
    std::vector<Primitive> primitives;
    std::vector<Buffer> buffers;
    std::vector<Material> materials;
    std::vector<Texture> textures;
    std::vector<Sampler> samplers;
    std::vector<Image> images;

    std::vector<GltfVertex> vertices;
    std::vector<u16> indices;

    std::vector<u32> nodes_preorder;
    std::vector<float4x4> cached_transforms;
};

Model load_model(const char *path);
} // namespace my_app
