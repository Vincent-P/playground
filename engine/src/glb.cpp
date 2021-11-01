#include "glb.h"
#if 0

#include <exo/prelude.h>
#include <exo/algorithms.h>
#include <exo/collections/vector.h>
#include <exo/logger.h>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <meshoptimizer.h>

#include <string_view>

namespace gltf
{
enum struct ComponentType : i32
{
    Byte          = 5120,
    UnsignedByte  = 5121,
    Short         = 5122,
    UnsignedShort = 5123,
    UnsignedInt   = 5125,
    Float         = 5126,
    Invalid
};

inline u32 size_of(ComponentType type)
{
    switch (type)
    {
    case ComponentType::Byte:
    case ComponentType::UnsignedByte:
        return 1;

    case ComponentType::Short:
    case ComponentType::UnsignedShort:
        return 2;

    case ComponentType::UnsignedInt:
    case ComponentType::Float:
        return 4;

    default:
        ASSERT(false);
        return 4;
    }
}
} // namespace gltf

namespace glb
{

enum struct ChunkType : u32
{
    Json   = 0x4E4F534A,
    Binary = 0x004E4942,
    Invalid,
};

struct Chunk
{
    u32       length;
    ChunkType type;
    u8        data[0];
};
static_assert(sizeof(Chunk) == 2 * sizeof(u32));

struct Header
{
    u32   magic;
    u32   version;
    u32   length;
    Chunk first_chunk;
};

struct Accessor
{
    gltf::ComponentType component_type   = gltf::ComponentType::Invalid;
    u32                 count            = 0;
    u32                 nb_component     = 0;
    u32                 bufferview_index = 0;
    u32                 byte_offset      = 0;
    float               min_float;
    float               max_float;
};

struct BufferView
{
    u32 byte_offset = 0;
    u32 byte_length = 1;
    u32 byte_stride = 0;
};

static Accessor get_accessor(const rapidjson::Value &object)
{
    const auto &accessor = object.GetObj();

    Accessor res = {};

    // technically not required but it doesn't make sense not to have one
    res.bufferview_index = accessor["bufferView"].GetUint();
    res.byte_offset      = 0;

    if (accessor.HasMember("byteOffset"))
    {
        res.byte_offset = accessor["byteOffset"].GetUint();
    }

    res.component_type = gltf::ComponentType(accessor["componentType"].GetInt());

    res.count = accessor["count"].GetUint();

    auto type = std::string_view(accessor["type"].GetString());
    if (type == "SCALAR")
    {
        res.nb_component = 1;
    }
    else if (type == "VEC2")
    {
        res.nb_component = 2;
    }
    else if (type == "VEC3")
    {
        res.nb_component = 3;
    }
    else if (type == "VEC4")
    {
        res.nb_component = 4;
    }
    else if (type == "MAT2")
    {
        res.nb_component = 4;
    }
    else if (type == "MAT3")
    {
        res.nb_component = 9;
    }
    else if (type == "MAT4")
    {
        res.nb_component = 16;
    }
    else
    {
        ASSERT(false);
    }

    return res;
}

static BufferView get_bufferview(const rapidjson::Value &object)
{
    const auto &bufferview = object.GetObj();
    BufferView  res        = {};
    if (bufferview.HasMember("byteOffset"))
    {
        res.byte_offset = bufferview["byteOffset"].GetUint();
    }
    res.byte_length = bufferview["byteLength"].GetUint();
    if (bufferview.HasMember("byteStride"))
    {
        res.byte_stride = bufferview["byteStride"].GetUint();
    }
    return res;
}

static void process_meshes(Scene &new_scene, rapidjson::Document &j_document, const Chunk *binary_chunk, Vec<usize> &mesh_remap)
{
    const auto &j_accessors   = j_document["accessors"].GetArray();
    const auto &j_bufferviews = j_document["bufferViews"].GetArray();

    if (j_document.HasMember("meshes"))
    {
        mesh_remap.resize(j_document["meshes"].GetArray().Size());
        usize i_current_mesh = 0;

        for (auto &j_mesh : j_document["meshes"].GetArray())
        {
            Mesh new_mesh = {};

            for (auto &j_primitive : j_mesh["primitives"].GetArray())
            {
                ASSERT(j_primitive.HasMember("attributes"));
                const auto &j_attributes = j_primitive["attributes"].GetObj();
                ASSERT(j_attributes.HasMember("POSITION"));

                new_mesh.submeshes.emplace_back();
                auto &new_submesh = new_mesh.submeshes.back();

                new_submesh.index_count  = 0;
                new_submesh.first_vertex = static_cast<u32>(new_mesh.positions.size());
                new_submesh.first_index  = static_cast<u32>(new_mesh.indices.size());

                if (j_primitive.HasMember("material"))
                {
                    new_submesh.i_material = j_primitive["material"].GetUint() + 1; // #0 is the default one for primitives without materials
                }

                // -- Attributes
                ASSERT(j_primitive.HasMember("indices"));
                {
                    auto  j_accessor  = j_accessors[j_primitive["indices"].GetUint()].GetObj();
                    auto  accessor    = get_accessor(j_accessor);
                    auto  bufferview  = get_bufferview(j_bufferviews[accessor.bufferview_index]);
                    usize byte_stride = bufferview.byte_stride > 0 ? bufferview.byte_stride : gltf::size_of(accessor.component_type) * accessor.nb_component;

                    // Copy the data from the binary buffer
                    new_mesh.indices.reserve(accessor.count);
                    for (usize i_index = 0; i_index < usize(accessor.count); i_index += 1)
                    {
                        u32   index;
                        usize offset = bufferview.byte_offset + accessor.byte_offset + i_index * byte_stride;
                        if (accessor.component_type == gltf::ComponentType::UnsignedShort)
                        {
                            index = new_submesh.first_vertex + *reinterpret_cast<const u16 *>(ptr_offset(binary_chunk->data, offset));
                        }
                        else if (accessor.component_type == gltf::ComponentType::UnsignedInt)
                        {
                            index = new_submesh.first_vertex + *reinterpret_cast<const u32 *>(ptr_offset(binary_chunk->data, offset));
                        }
                        else
                        {
                            ASSERT(false);
                        }
                        new_mesh.indices.push_back(index);
                    }

                    new_submesh.index_count = accessor.count;
                }

                usize vertex_count = 0;
                {
                    auto j_accessor   = j_accessors[j_attributes["POSITION"].GetUint()].GetObj();
                    auto accessor     = get_accessor(j_accessor);
                    vertex_count      = accessor.count;
                    auto  bufferview  = get_bufferview(j_bufferviews[accessor.bufferview_index]);
                    usize byte_stride = bufferview.byte_stride > 0 ? bufferview.byte_stride : gltf::size_of(accessor.component_type) * accessor.nb_component;

                    // Copy the data from the binary buffer
                    new_mesh.positions.reserve(accessor.count);
                    for (usize i_position = 0; i_position < usize(accessor.count); i_position += 1)
                    {
                        usize  offset       = bufferview.byte_offset + accessor.byte_offset + i_position * byte_stride;
                        float4 new_position = {1.0f};

                        if (accessor.component_type == gltf::ComponentType::UnsignedShort)
                        {
                            const auto *components = reinterpret_cast<const u16 *>(ptr_offset(binary_chunk->data, offset));
                            new_position           = {float(components[0]), float(components[1]), float(components[2]), 1.0f};
                        }
                        else if (accessor.component_type == gltf::ComponentType::Float)
                        {
                            const auto *components = reinterpret_cast<const float *>(ptr_offset(binary_chunk->data, offset));
                            new_position           = {float(components[0]), float(components[1]), float(components[2]), 1.0f};
                        }
                        else
                        {
                            ASSERT(false);
                        }

                        new_mesh.positions.push_back(new_position);
                    }
                }

                if (j_attributes.HasMember("TEXCOORD_0"))
                {
                    auto j_accessor = j_accessors[j_attributes["TEXCOORD_0"].GetUint()].GetObj();
                    auto accessor   = get_accessor(j_accessor);
                    ASSERT(accessor.count == vertex_count);
                    auto  bufferview  = get_bufferview(j_bufferviews[accessor.bufferview_index]);
                    usize byte_stride = bufferview.byte_stride > 0 ? bufferview.byte_stride : gltf::size_of(accessor.component_type) * accessor.nb_component;

                    // Copy the data from the binary buffer
                    new_mesh.uvs.reserve(accessor.count);
                    for (usize i_uv = 0; i_uv < usize(accessor.count); i_uv += 1)
                    {
                        usize  offset = bufferview.byte_offset + accessor.byte_offset + i_uv * byte_stride;
                        float2 new_uv = {1.0f};

                        if (accessor.component_type == gltf::ComponentType::UnsignedShort)
                        {
                            const auto *components = reinterpret_cast<const u16 *>(ptr_offset(binary_chunk->data, offset));
                            new_uv                 = {float(components[0]), float(components[1])};
                        }
                        else if (accessor.component_type == gltf::ComponentType::Float)
                        {
                            const auto *components = reinterpret_cast<const float *>(ptr_offset(binary_chunk->data, offset));
                            new_uv                 = {float(components[0]), float(components[1])};
                        }
                        else
                        {
                            ASSERT(false);
                        }

                        new_mesh.uvs.push_back(new_uv);
                    }
                }
                else
                {
                    new_mesh.uvs.reserve(vertex_count);
                    for (usize i = 0; i < vertex_count; i += 1)
                    {
                        new_mesh.uvs.push_back(float2(0.0f, 0.0f));
                    }
                }
            }

            // Merge similar meshes
            usize i_similar_mesh = u64_invalid;
            for (usize i_mesh = 0; i_mesh < new_scene.meshes.size(); i_mesh += 1)
            {
                const auto &mesh = new_scene.meshes[i_mesh];
                if (mesh.is_similar(new_mesh))
                {
                    i_similar_mesh = i_mesh;
                    break;
                }
            }

            if (i_similar_mesh == u64_invalid)
            {
                mesh_remap[i_current_mesh] = new_scene.meshes.size();
                new_scene.meshes.push_back(std::move(new_mesh));
            }
            else
            {
                mesh_remap[i_current_mesh] = i_similar_mesh;
            }

            i_current_mesh += 1;
        }

        logger::info("Loaded {} unique meshes from {} meshes in file.\n", new_scene.meshes.size(), mesh_remap.size());
    }
}

static void process_images(Scene &new_scene, rapidjson::Document &j_document, const Chunk *binary_chunk)
{
    const auto &j_bufferviews = j_document["bufferViews"].GetArray();

    if (j_document.HasMember("images"))
    {
        for (auto &j_image : j_document["images"].GetArray())
        {
            if (!j_image.HasMember("mimeType") || !j_image.HasMember("bufferView"))
            {
                break;
            }

            std::string_view mime_type        = j_image["mimeType"].GetString();
            auto             bufferview_index = j_image["bufferView"].GetUint();
            auto             bufferview       = get_bufferview(j_bufferviews[bufferview_index]);

            const u8 *image_data = reinterpret_cast<const u8 *>(ptr_offset(binary_chunk->data, bufferview.byte_offset));
            usize     size       = bufferview.byte_length;

            if (mime_type == "image/ktx2")
            {
                new_scene.textures.push_back(Texture::from_ktx2(image_data, size));
            }
            else if (mime_type == "image/png")
            {
                new_scene.textures.push_back(Texture::from_png(image_data, size));
            }
        }
    }

}

static void process_materials(Scene &new_scene, rapidjson::Document &j_document, const Chunk *)
{
    // add a fallback material at index 0
    new_scene.materials.emplace_back();
    if (j_document.HasMember("materials"))
    {
        for (const auto &j_material : j_document["materials"].GetArray())
        {
            Material new_material = {};
            if (j_material.HasMember("pbrMetallicRoughness"))
            {
                const auto &j_pbr = j_material["pbrMetallicRoughness"].GetObj();

                if (j_pbr.HasMember("baseColorTexture"))
                {
                    const auto &j_base_color_texture = j_pbr["baseColorTexture"];
                    u32         texture_index        = j_base_color_texture["index"].GetUint();

                    const auto &j_texture = j_document["textures"][texture_index];
                    // texture: {"extensions":{"KHR_texture_basisu":{"source":0}}}

                    bool has_extension = false;
                    if (j_texture.HasMember("extensions"))
                    {
                        for (const auto &j_extension : j_texture["extensions"].GetObj())
                        {
                            if (std::string_view(j_extension.name.GetString()) == std::string_view("KHR_texture_basisu"))
                            {
                                has_extension = true;
                                break;
                            }
                        }
                    }

                    if (!has_extension)
                    {
                        logger::error("[GLB] Material references a texture that isn't in basis format.\n");
                    }
                    else
                    {
                        new_material.base_color_texture = j_texture["extensions"]["KHR_texture_basisu"]["source"].GetUint();
                    }

                    // TODO: Assert that all textures of this material have the same texture transform
                    if (j_base_color_texture.HasMember("extensions") && j_base_color_texture["extensions"].HasMember("KHR_texture_transform"))
                    {
                        const auto &extension = j_base_color_texture["extensions"]["KHR_texture_transform"];
                        if (extension.HasMember("offset"))
                        {
                            new_material.offset[0] = extension["offset"].GetArray()[0].GetFloat();
                            new_material.offset[1] = extension["offset"].GetArray()[1].GetFloat();
                        }
                        if (extension.HasMember("scale"))
                        {
                            new_material.scale[0] = extension["scale"].GetArray()[0].GetFloat();
                            new_material.scale[1] = extension["scale"].GetArray()[1].GetFloat();
                        }
                        if (extension.HasMember("rotation"))
                        {
                            new_material.rotation = extension["rotation"].GetFloat();
                        }
                    }
                }

                if (j_pbr.HasMember("baseColorFactor"))
                {
                    new_material.base_color_factor = {1.0, 1.0, 1.0, 1.0};
                    for (u32 i = 0; i < 4; i += 1)
                    {
                        new_material.base_color_factor[i] = j_pbr["baseColorFactor"].GetArray()[i].GetFloat();
                    }
                }
            }

            new_scene.materials.push_back(std::move(new_material));
        }
    }
}

static void process_nodes(Scene &new_scene, rapidjson::Document &j_document, const Chunk *, u32 i_scene, const Vec<usize> &mesh_remap)
{
    auto j_scenes = j_document["scenes"].GetArray();
    auto j_scene = j_scenes[i_scene].GetObj();

    auto j_nodes  = j_document["nodes"].GetArray();
    auto j_roots = j_scene["nodes"].GetArray();

    Vec<u32>      i_node_stack;
    Vec<float4x4> transforms_stack;
    i_node_stack.reserve(j_nodes.Size());
    transforms_stack.reserve(j_nodes.Size());

    auto get_transform = [](auto j_node) {
        float4x4 transform = float4x4::identity();

        if (j_node.HasMember("matrix"))
        {
            usize i      = 0;
            auto  matrix = j_node["matrix"].GetArray();
            ASSERT(matrix.Size() == 16);

            for (u32 i_element = 0; i_element < matrix.Size(); i_element += 1)
            {
                transform.at(i % 4, i / 4) = static_cast<float>(matrix[i_element].GetDouble());
            }
        }

        if (j_node.HasMember("translation"))
        {
            auto     translation_factors = j_node["translation"].GetArray();
            float4x4 translation         = float4x4::identity();
            translation.at(0, 3)         = static_cast<float>(translation_factors[0].GetDouble());
            translation.at(1, 3)         = static_cast<float>(translation_factors[1].GetDouble());
            translation.at(2, 3)         = static_cast<float>(translation_factors[2].GetDouble());
            transform                    = translation;
        }

        if (j_node.HasMember("rotation"))
        {
            auto   rotation = j_node["rotation"].GetArray();
            float4 quaternion = {0.0f};
            quaternion.x = static_cast<float>(rotation[0].GetDouble());
            quaternion.y = static_cast<float>(rotation[1].GetDouble());
            quaternion.z = static_cast<float>(rotation[2].GetDouble());
            quaternion.w = static_cast<float>(rotation[3].GetDouble());

            transform = transform
                        * float4x4({
                            1.0f - 2.0f * quaternion.y * quaternion.y - 2.0f * quaternion.z * quaternion.z,
                            2.0f * quaternion.x * quaternion.y - 2.0f * quaternion.z * quaternion.w,
                            2.0f * quaternion.x * quaternion.z + 2.0f * quaternion.y * quaternion.w,
                            0.0f,
                            2.0f * quaternion.x * quaternion.y + 2.0f * quaternion.z * quaternion.w,
                            1.0f - 2.0f * quaternion.x * quaternion.x - 2.0f * quaternion.z * quaternion.z,
                            2.0f * quaternion.y * quaternion.z - 2.0f * quaternion.x * quaternion.w,
                            0.0f,
                            2.0f * quaternion.x * quaternion.z - 2.0f * quaternion.y * quaternion.w,
                            2.0f * quaternion.y * quaternion.z + 2.0f * quaternion.x * quaternion.w,
                            1.0f - 2.0f * quaternion.x * quaternion.x - 2.0f * quaternion.y * quaternion.y,
                            0.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            1.0f,
                        });
        }

        if (j_node.HasMember("scale"))
        {
            auto     scale_factors = j_node["scale"].GetArray();
            float4x4 scale         = {};
            scale.at(0, 0)         = static_cast<float>(scale_factors[0].GetDouble());
            scale.at(1, 1)         = static_cast<float>(scale_factors[1].GetDouble());
            scale.at(2, 2)         = static_cast<float>(scale_factors[2].GetDouble());
            scale.at(3, 3)         = 1.0f;

            transform = transform * scale;
        }

        return transform;
    };

    for (auto &root : j_roots)
    {
        u32 i_root = root.GetUint();

        i_node_stack.clear();
        transforms_stack.clear();

        i_node_stack.push_back(i_root);
        transforms_stack.push_back(float4x4::identity());

        while (!i_node_stack.empty())
        {
            u32 i_node = i_node_stack.back();
            i_node_stack.pop_back();
            float4x4 parent_transform = transforms_stack.back();
            transforms_stack.pop_back();

            auto j_node    = j_nodes[i_node].GetObj();
            auto transform = parent_transform * get_transform(j_node);

            if (j_node.HasMember("mesh"))
            {
                new_scene.instances.push_back({
                    .i_mesh    = static_cast<u32>(mesh_remap[j_node["mesh"].GetUint()]),
                    .transform = transform,
                });
            }

            if (j_node.HasMember("children"))
            {
                auto children = j_node["children"].GetArray();

                for (u32 i_child = 0; i_child < children.Size(); i_child += 1)
                {
                    i_node_stack.push_back(children[i_child].GetUint());
                    transforms_stack.push_back(transform);
                }
            }
        }
    }
}

static void process_json(Scene &new_scene, rapidjson::Document &j_document, const Chunk *binary_chunk)
{
    Vec<usize> mesh_remap;
    process_meshes(new_scene, j_document, binary_chunk, mesh_remap);
    process_images(new_scene, j_document, binary_chunk);
    process_materials(new_scene, j_document, binary_chunk);

    u32 i_scene = 0;
    if (j_document.HasMember("scene"))
    {
        i_scene = j_document["scene"].GetUint();
    }

    process_nodes(new_scene, j_document, binary_chunk, i_scene, mesh_remap);
}

Scene load_file(const std::string_view &path)
{
    auto file = cross::MappedFile::open(path);
    if (!file)
    {
        return {};
    }

    Scene scene = {};

    scene.file = std::move(*file);

    const auto &header = *reinterpret_cast<const Header *>(scene.file.base_addr);
    if (header.magic != 0x46546C67)
    {
        logger::error("[GLB] Invalid GLB file.\n");
        return {};
    }

    if (header.first_chunk.type != ChunkType::Json)
    {
        logger::error("[GLB] First chunk isn't JSON.\n");
        return {};
    }

    std::string_view json_content{reinterpret_cast<const char *>(&header.first_chunk.data), header.first_chunk.length};

    rapidjson::Document document;
    document.Parse(json_content.data(), json_content.size());

    if (document.HasParseError())
    {
        logger::error("[GLB] JSON Error at offset {}: {}\n", document.GetErrorOffset(), rapidjson::GetParseError_En(document.GetParseError()));
        return {};
    }

    const Chunk *binary_chunk = nullptr;
    if (sizeof(Header) + header.first_chunk.length < header.length)
    {
        binary_chunk = reinterpret_cast<const Chunk *>(ptr_offset(header.first_chunk.data, header.first_chunk.length));
        if (binary_chunk->type != ChunkType::Binary)
        {
            logger::error("[GLB] Second chunk isn't BIN.\n");
            return {};
        }
    }

    process_json(scene, document, binary_chunk);

    return scene;
}
} // namespace glb
#endif
