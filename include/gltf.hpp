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

struct BufferView
{
    u32 buffer;
    u32 byte_offset;
    u32 byte_length;

    u32 byte_stride;
    u32 target;
};

enum class AccessorType
{
    Scalar,
    Vec3,
    Vec4,
    Mat4,
    Unkown
};

std::optional<AccessorType> accessor_type_from_str(const std::string& string);

// https://gist.github.com/szimek/763999
enum class ComponentType
{
    Byte = 5120,
    UnsignedByte = 5121,
    Short = 5122,
    UnsignedShort = 5123,
    Int = 5124,
    UnsignedInt = 5125,
    Float = 5126,
};

struct Accessor
{
    u32 buffer_view;
    u32 byte_offset;
    u32 count;
    ComponentType component_type;
    AccessorType type;
};

struct Primitive
{
    u32           position_accessor;
    u32           normal_accessor;
    u32           indices_accessor;
    u32           uv0_accessor;
    u32           uv1_accessor;
    RenderingMode mode;
    u32           material;
};

struct Mesh
{
    const char *           name;
    std::vector<Primitive> primitives;
};

struct Model
{
    std::vector<Mesh>       meshes;
    std::vector<Primitive>  primitives;
    std::vector<Accessor>   accessors;
    std::vector<Material>   materials;
    std::vector<BufferView> buffer_views;
    std::vector<Buffer>     buffers;
};

Model load_model(const char *path);
} // namespace my_app
