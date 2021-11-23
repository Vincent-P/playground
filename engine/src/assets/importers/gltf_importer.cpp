#include "assets/importers/gltf_importer.h"

#include <exo/prelude.h>
#include <exo/logger.h>

#include "assets/asset_manager.h"
#include "assets/subscene.h"
#include "assets/mesh.h"
#include "assets/texture.h"
#include "assets/material.h"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/filewritestream.h>

struct ImportContext
{
    AssetManager *asset_manager;
    SubScene *new_scene;
    const rapidjson::Document &j_document;
    const void* binary_chunk;
    GLTFImporter::Data &importer_data;
};

static void import_meshes(ImportContext &ctx);
static void import_nodes(ImportContext &ctx);
static void import_materials(ImportContext &ctx);

// -- glTF data utils

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

// -- glTF data utils end


bool GLTFImporter::can_import(const void *file_data, usize file_len)
{
    ASSERT(file_len > 4);
    UNUSED(file_len);
    const char *as_char = reinterpret_cast<const char*>(file_data);
    return as_char[0] == 'g' && as_char[1] == 'l' && as_char[2] == 'T' && as_char[3] == 'F';
}

Result<Asset*> GLTFImporter::import(AssetManager *asset_manager, cross::UUID resource_uuid, const void *file_data, usize file_len, void *importer_data)
{
    UNUSED(file_len);

    const auto &header = *reinterpret_cast<const Header *>(file_data);
    if (header.first_chunk.type != ChunkType::Json)
    {
        return Err(GLTFError::FirstChunkNotJSON);
    }

    std::string_view json_content{reinterpret_cast<const char *>(&header.first_chunk.data), header.first_chunk.length};
    rapidjson::Document document;
    document.Parse(json_content.data(), json_content.size());
    if (document.HasParseError())
    {
        return Err(AssetErrors::ParsingError, JsonError{document.GetErrorOffset(), rapidjson::GetParseError_En(document.GetParseError())});
    }

    ASSERT(sizeof(Header) + header.first_chunk.length < header.length);
    const auto *binary_chunk = reinterpret_cast<const Chunk *>(ptr_offset(header.first_chunk.data, header.first_chunk.length));
    if (binary_chunk->type != ChunkType::Binary)
    {
        return Err(GLTFError::SecondChunkNotBIN);
    }

    auto *new_scene = asset_manager->create_asset<SubScene>(resource_uuid);

    ASSERT(importer_data);
    ImportContext ctx = {
        .asset_manager = asset_manager,
        .new_scene     = new_scene,
        .j_document    = document,
        .binary_chunk  = binary_chunk->data,
        .importer_data = *reinterpret_cast<GLTFImporter::Data*>(importer_data),
    };

    import_materials(ctx);
    import_meshes(ctx);
    import_nodes(ctx);

    asset_manager->save_asset(new_scene);

    return new_scene;
}

static void import_meshes(ImportContext &ctx)
{
    const auto &j_accessors   = ctx.j_document["accessors"].GetArray();
    const auto &j_bufferviews = ctx.j_document["bufferViews"].GetArray();

    usize mesh_count = 0;

    if (!ctx.j_document.HasMember("meshes"))
    {
        return;
    }

    const auto &j_meshes = ctx.j_document["meshes"].GetArray();

    // Generate new UUID for the meshes if needed
    auto &mesh_uuids = ctx.importer_data.mesh_uuids;
    if (mesh_uuids.size() != j_meshes.Size())
    {
        mesh_uuids.resize(j_meshes.Size());

        for (u32 i_mesh = 0; i_mesh < j_meshes.Size(); i_mesh += 1)
        {
            if (!mesh_uuids[i_mesh].is_valid())
            {
                mesh_uuids[i_mesh] = cross::UUID::create();
            }
        }
    }

    for (u32 i_mesh = 0; i_mesh < j_meshes.Size(); i_mesh += 1)
    {
        // TODO: Check if mesh needs to be re-imported or not

        // hash = ...compute hash...
        // if hash == asset_manager->asset_metadatas[mesh_uuid].hash:
        //   continue;

        const auto &j_mesh   = j_meshes[i_mesh];
        auto *      new_mesh = ctx.asset_manager->create_asset<Mesh>(mesh_uuids[i_mesh]);

        if (j_mesh.HasMember("name"))
        {
            new_mesh->name = std::string{j_mesh["name"].GetString()};
        }

        for (auto &j_primitive : j_mesh["primitives"].GetArray())
        {
            ASSERT(j_primitive.HasMember("attributes"));
            const auto &j_attributes = j_primitive["attributes"].GetObj();
            ASSERT(j_attributes.HasMember("POSITION"));

            new_mesh->submeshes.emplace_back();
            auto &new_submesh = new_mesh->submeshes.back();

            new_submesh.index_count  = 0;
            new_submesh.first_vertex = static_cast<u32>(new_mesh->positions.size());
            new_submesh.first_index  = static_cast<u32>(new_mesh->indices.size());
            new_submesh.material     = {};

            // -- Attributes
            ASSERT(j_primitive.HasMember("indices"));
            {
                auto  j_accessor  = j_accessors[j_primitive["indices"].GetUint()].GetObj();
                auto  accessor    = get_accessor(j_accessor);
                auto  bufferview  = get_bufferview(j_bufferviews[accessor.bufferview_index]);
                usize byte_stride = bufferview.byte_stride > 0 ? bufferview.byte_stride : gltf::size_of(accessor.component_type) * accessor.nb_component;

                // Copy the data from the binary buffer
                new_mesh->indices.reserve(accessor.count);
                for (usize i_index = 0; i_index < usize(accessor.count); i_index += 1)
                {
                    u32   index;
                    usize offset = bufferview.byte_offset + accessor.byte_offset + i_index * byte_stride;
                    if (accessor.component_type == gltf::ComponentType::UnsignedShort)
                    {
                        index = new_submesh.first_vertex + *reinterpret_cast<const u16 *>(ptr_offset(ctx.binary_chunk, offset));
                    }
                    else if (accessor.component_type == gltf::ComponentType::UnsignedInt)
                    {
                        index = new_submesh.first_vertex + *reinterpret_cast<const u32 *>(ptr_offset(ctx.binary_chunk, offset));
                    }
                    else
                    {
                        ASSERT(false);
                    }
                    new_mesh->indices.push_back(index);
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
                new_mesh->positions.reserve(accessor.count);
                for (usize i_position = 0; i_position < usize(accessor.count); i_position += 1)
                {
                    usize  offset       = bufferview.byte_offset + accessor.byte_offset + i_position * byte_stride;
                    float4 new_position = {1.0f};

                    if (accessor.component_type == gltf::ComponentType::UnsignedShort)
                    {
                        const auto *components = reinterpret_cast<const u16 *>(ptr_offset(ctx.binary_chunk, offset));
                        new_position           = {float(components[0]), float(components[1]), float(components[2]), 1.0f};
                    }
                    else if (accessor.component_type == gltf::ComponentType::Float)
                    {
                        const auto *components = reinterpret_cast<const float *>(ptr_offset(ctx.binary_chunk, offset));
                        new_position           = {float(components[0]), float(components[1]), float(components[2]), 1.0f};
                    }
                    else
                    {
                        ASSERT(false);
                    }

                    new_mesh->positions.push_back(new_position);
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
                new_mesh->uvs.reserve(accessor.count);
                for (usize i_uv = 0; i_uv < usize(accessor.count); i_uv += 1)
                {
                    usize  offset = bufferview.byte_offset + accessor.byte_offset + i_uv * byte_stride;
                    float2 new_uv = {1.0f};

                    if (accessor.component_type == gltf::ComponentType::UnsignedShort)
                    {
                        const auto *components = reinterpret_cast<const u16 *>(ptr_offset(ctx.binary_chunk, offset));
                        new_uv                 = {float(components[0]), float(components[1])};
                    }
                    else if (accessor.component_type == gltf::ComponentType::Float)
                    {
                        const auto *components = reinterpret_cast<const float *>(ptr_offset(ctx.binary_chunk, offset));
                        new_uv                 = {float(components[0]), float(components[1])};
                    }
                    else
                    {
                        ASSERT(false);
                    }

                    new_mesh->uvs.push_back(new_uv);
                }
            }
            else
            {
                new_mesh->uvs.reserve(vertex_count);
                for (usize i = 0; i < vertex_count; i += 1)
                {
                    new_mesh->uvs.push_back(float2(0.0f, 0.0f));
                }
            }

            if (j_primitive.HasMember("material"))
            {
                u32 i_material       = j_primitive["material"].GetUint();
                new_submesh.material = ctx.importer_data.material_uuids[i_material];
            }
        }

        ctx.asset_manager->save_asset(new_mesh);
        ctx.new_scene->dependencies.push_back(new_mesh->uuid);
        mesh_count += 1;
    }

    logger::info("[GLTF Importer] Imported {} meshes.\n", mesh_count);
}

static void import_nodes(ImportContext &ctx)
{
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


    u32 i_scene = ctx.j_document.HasMember("scene") ? ctx.j_document["scene"].GetUint() : ctx.importer_data.settings.i_scene;
    const auto &j_scenes = ctx.j_document["scenes"].GetArray();
    const auto &j_scene = j_scenes[i_scene].GetObj();
    const auto &j_roots = j_scene["nodes"].GetArray();

    for (const auto &j_root : j_roots)
    {
        ctx.new_scene->roots.push_back(j_root.GetUint());
    }

    const auto &j_nodes  = ctx.j_document["nodes"].GetArray();

    ctx.new_scene->transforms.reserve(j_nodes.Size());
    ctx.new_scene->meshes.reserve(j_nodes.Size());
    ctx.new_scene->children.reserve(j_nodes.Size());
    ctx.new_scene->names.reserve(j_nodes.Size());

    for (const auto &j_node : j_nodes)
    {
        const auto &j_node_obj = j_node.GetObj();

        ctx.new_scene->transforms.push_back(get_transform(j_node_obj));

        if (j_node.HasMember("mesh"))
        {
            auto i_mesh = j_node["mesh"].GetUint();
            ctx.new_scene->meshes.push_back(ctx.importer_data.mesh_uuids[i_mesh]);
        }
        else
        {
            ctx.new_scene->meshes.emplace_back();
        }

        ctx.new_scene->names.emplace_back();
        if (j_node.HasMember("name"))
        {
            ctx.new_scene->names.back() = std::string{j_node["name"].GetString()};
        }

        ctx.new_scene->children.emplace_back();
        if (j_node.HasMember("children"))
        {
            const auto &j_children = j_node["children"].GetArray();
            ctx.new_scene->children.back().reserve(j_children.Size());
            for (const auto &j_child : j_children)
            {
                ctx.new_scene->children.back().push_back(j_child.GetUint());
            }
        }
    }
}

static void import_materials(ImportContext &ctx)
{
    const auto &j_document = ctx.j_document;

    if (!ctx.j_document.HasMember("materials"))
    {
        return;
    }

    const auto &j_materials = ctx.j_document["materials"].GetArray();
    // Generate new UUID for the materials if needed
    auto &material_uuids = ctx.importer_data.material_uuids;
    if (material_uuids.size() != j_materials.Size())
    {
        material_uuids.resize(j_materials.Size());

        for (u32 i_mesh = 0; i_mesh < j_materials.Size(); i_mesh += 1)
        {
            if (!material_uuids[i_mesh].is_valid())
            {
                material_uuids[i_mesh] = cross::UUID::create();
            }
        }
    }

    ctx.new_scene->materials = material_uuids;

    for (usize i_material = 0; i_material < j_materials.Size(); i_material += 1)
    {
        const auto &j_material = j_materials[i_material];

        Material *new_material = ctx.asset_manager->create_asset<Material>(material_uuids[i_material]);

        if (j_material.HasMember("name"))
        {
            new_material->name = std::string{j_material["name"].GetString()};
        }

        if (j_material.HasMember("pbrMetallicRoughness"))
        {
            const auto &j_pbr = j_material["pbrMetallicRoughness"].GetObj();

            if (j_pbr.HasMember("baseColorTexture"))
            {
                const auto &j_base_color_texture = j_pbr["baseColorTexture"];
                u32         texture_index        = j_base_color_texture["index"].GetUint();

                new_material->base_color_texture = {};

                // TODO: Assert that all textures of this material have the same texture transform
                if (j_base_color_texture.HasMember("extensions") && j_base_color_texture["extensions"].HasMember("KHR_texture_transform"))
                {
                    const auto &extension = j_base_color_texture["extensions"]["KHR_texture_transform"];
                    if (extension.HasMember("offset"))
                    {
                        new_material->uv_transform.offset[0] = extension["offset"].GetArray()[0].GetFloat();
                        new_material->uv_transform.offset[1] = extension["offset"].GetArray()[1].GetFloat();
                    }
                    if (extension.HasMember("scale"))
                    {
                        new_material->uv_transform.scale[0] = extension["scale"].GetArray()[0].GetFloat();
                        new_material->uv_transform.scale[1] = extension["scale"].GetArray()[1].GetFloat();
                    }
                    if (extension.HasMember("rotation"))
                    {
                        new_material->uv_transform.rotation = extension["rotation"].GetFloat();
                    }
                }
            }

            if (j_pbr.HasMember("baseColorFactor"))
            {
                new_material->base_color_factor = {1.0, 1.0, 1.0, 1.0};
                for (u32 i = 0; i < 4; i += 1)
                {
                    new_material->base_color_factor[i] = j_pbr["baseColorFactor"].GetArray()[i].GetFloat();
                }
            }
        }
        ctx.asset_manager->save_asset(new_material);
        ctx.new_scene->dependencies.push_back(new_material->uuid);
    }
}

void *GLTFImporter::create_default_importer_data()
{
    return reinterpret_cast<void *>(new GLTFImporter::Data());
}

void *GLTFImporter::read_data_json(const rapidjson::Value &j_data)
{
    auto *new_data = create_default_importer_data();
    auto *data     = reinterpret_cast<GLTFImporter::Data *>(new_data);

    const auto &j_settings = j_data["settings"].GetObj();

    data->settings = {};

    if (j_settings.HasMember("i_scene"))
    {
        data->settings.i_scene = j_settings["i_scene"].GetUint();
    }
    if (j_settings.HasMember("apply_transform"))
    {
        data->settings.apply_transform = j_settings["apply_transform"].GetBool();
    }
    if (j_settings.HasMember("remove_degenerate_triangles"))
    {
        data->settings.remove_degenerate_triangles = j_settings["remove_degenerate_triangles"].GetBool();
    }

    if (j_data.HasMember("mesh_uuids"))
    {
        const auto &j_mesh_uuids = j_data["mesh_uuids"].GetArray();
        data->mesh_uuids.reserve(j_mesh_uuids.Size());
        for (const auto &j_mesh_uuid : j_mesh_uuids)
        {
            auto mesh_uuid = cross::UUID::from_string(j_mesh_uuid.GetString(), j_mesh_uuid.GetStringLength());
            data->mesh_uuids.push_back(mesh_uuid);
        }
    }

    if (j_data.HasMember("texture_uuids"))
    {
        const auto &j_texture_uuids = j_data["texture_uuids"].GetArray();
        data->texture_uuids.reserve(j_texture_uuids.Size());
        for (const auto &j_texture_uuid : j_texture_uuids)
        {
            auto texture_uuid = cross::UUID::from_string(j_texture_uuid.GetString(), j_texture_uuid.GetStringLength());
            data->texture_uuids.push_back(texture_uuid);
        }
    }

    if (j_data.HasMember("material_uuids"))
    {
        const auto &j_material_uuids = j_data["material_uuids"].GetArray();
        data->material_uuids.reserve(j_material_uuids.Size());
        for (const auto &j_material_uuid : j_material_uuids)
        {
            auto material_uuid
                = cross::UUID::from_string(j_material_uuid.GetString(), j_material_uuid.GetStringLength());
            data->material_uuids.push_back(material_uuid);
        }
    }

    return new_data;
}

void GLTFImporter::write_data_json(rapidjson::GenericPrettyWriter<rapidjson::FileWriteStream> &writer, const void *data)
{
    ASSERT(data);
    const auto *import_data = reinterpret_cast<const GLTFImporter::Data *>(data);

    writer.StartObject();

    writer.Key("settings");
    writer.StartObject();
    writer.Key("i_scene");
    writer.Uint(import_data->settings.i_scene);
    writer.Key("apply_transform");
    writer.Bool(import_data->settings.apply_transform);
    writer.Key("remove_degenerate_triangles");
    writer.Bool(import_data->settings.remove_degenerate_triangles);
    writer.EndObject();

    writer.Key("mesh_uuids");
    writer.StartArray();
    for (const auto &mesh_uuid : import_data->mesh_uuids)
    {
        writer.String(mesh_uuid.str, mesh_uuid.STR_LEN);
    }
    writer.EndArray();

    writer.Key("texture_uuids");
    writer.StartArray();
    for (const auto &texture_uuid : import_data->texture_uuids)
    {
        writer.String(texture_uuid.str, texture_uuid.STR_LEN);
    }
    writer.EndArray();

    writer.Key("material_uuids");
    writer.StartArray();
    for (const auto &material_uuid : import_data->material_uuids)
    {
        writer.String(material_uuid.str, material_uuid.STR_LEN);
    }
    writer.EndArray();

    writer.EndObject();
}
