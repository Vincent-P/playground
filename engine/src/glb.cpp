#include "glb.hpp"

#include "base/algorithms.hpp"
#include "base/numerics.hpp"
#include "base/types.hpp"
#include "base/logger.hpp"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <meshoptimizer.h>

namespace gltf
{
enum struct ComponentType : i32
{
    Byte = 5120,
    UnsignedByte = 5121,
    Short = 5122,
    UnsignedShort = 5123,
    UnsignedInt = 5125,
    Float = 5126,
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
        assert(false);
        return 4;
    }
}
}

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
    u32 length;
    ChunkType type;
    u8  data[0];
};
static_assert(sizeof(Chunk) == 2 * sizeof(u32));

struct Header
{
    u32 magic;
    u32 version;
    u32 length;
    Chunk first_chunk;
};

struct Accessor
{
    gltf::ComponentType component_type = gltf::ComponentType::Invalid;
    int count = 0;
    int nb_component = 0;
    int bufferview_index = 0;
    int byte_offset = 0;
    u32 min;
    u32 max;
    float min_float;
    float max_float;
};

struct BufferView
{
    int byte_offset = 0;
    int byte_length = 0;
};

static Accessor get_accessor(const rapidjson::Value &object)
{
    const auto &accessor = object.GetObject();

    Accessor res = {};

    // technically not required but it doesn't make sense not to have one
    res.bufferview_index = accessor["bufferView"].GetInt();
    res.byte_offset = 0;

    if (accessor.HasMember("byteOffset")) {
        res.byte_offset = accessor["byteOffset"].GetInt();
    }

    res.component_type = gltf::ComponentType(accessor["componentType"].GetInt());

    res.count = accessor["count"].GetInt();

    auto type = std::string_view(accessor["type"].GetString());
    if (type == "SCALAR") {
        res.nb_component = 1;
    }
    else if (type == "VEC2") {
        res.nb_component = 2;
    }
    else if (type == "VEC3") {
        res.nb_component = 3;
    }
    else if (type == "VEC4") {
        res.nb_component = 4;
    }
    else if (type == "MAT2") {
        res.nb_component = 4;
    }
    else if (type == "MAT3") {
        res.nb_component = 9;
    }
    else if (type == "MAT4") {
        res.nb_component = 16;
    }
    else {
        assert(false);
    }

    return res;
}

static BufferView get_bufferview(const rapidjson::Value &object)
{
    const auto &bufferview = object.GetObject();
    BufferView res = {};
    if (bufferview.HasMember("byteOffset")) {
        res.byte_offset = bufferview["byteOffset"].GetInt();
    }
    res.byte_length = bufferview["byteLength"].GetInt();
    return res;
}

static void process_json(Scene &scene, rapidjson::Document &document, const Chunk *binary_chunk)
{
    if (document.HasMember("extensionsRequired"))
    {
        logger::info("Extensions required:\n");
        for (const auto &extension : document["extensionsRequired"].GetArray())
        {
            logger::info("\t- {}\n", extension.GetString());
        }
    }

    if (document.HasMember("extensionsUsed"))
    {
        logger::info("Extensions used:\n");
        for (const auto &extension : document["extensionsUsed"].GetArray())
        {
            logger::info("\t- {}\n", extension.GetString());
        }
    }

    const auto &accessors = document["accessors"].GetArray();
    const auto &bufferviews = document["bufferViews"].GetArray();

    u32 index_acc = 0;

    if (document.HasMember("meshes"))
    {
        for (auto &mesh : document["meshes"].GetArray())
        {
            logger::info("Mesh\n");

            scene.meshes.emplace_back();
            auto &new_mesh = scene.meshes.back();

            usize i_primitive = 0;
            for (auto &primitive : mesh["primitives"].GetArray())

            {
                logger::info("\tprimitive #{}\n", i_primitive++);
                assert(primitive.HasMember("attributes"));
                const auto &attributes = primitive["attributes"].GetObject();
                assert(attributes.HasMember("POSITION"));

                new_mesh.submeshes.emplace_back();
                auto &new_submesh = new_mesh.submeshes.back();

                new_submesh.vertex_count = 0;
                new_submesh.index_count  = 0;
                new_submesh.first_vertex = static_cast<u32>(new_mesh.positions.size());
                new_submesh.first_index  = static_cast<u32>(new_mesh.indices.size());

                {
                    auto accessor = get_accessor(accessors[attributes["POSITION"].GetInt()]);
                    new_submesh.vertex_count = accessor.count;
                    auto bufferview = get_bufferview(bufferviews[accessor.bufferview_index]);

                    usize byte_stride = gltf::size_of(accessor.component_type) * accessor.nb_component;
                    assert(accessor.component_type == gltf::ComponentType::UnsignedShort);

                    assert(byte_stride == 3 * sizeof(u16));

                    auto u16_to_float = [](u16 unorm) -> float { return float(unorm) / 65535.0f; };

                    new_mesh.positions.reserve(bufferview.byte_length / byte_stride);
                    for (usize i_position = 0; i_position < usize(accessor.count); i_position += 1)
                    {
                        usize offset = bufferview.byte_offset + accessor.byte_offset + i_position * byte_stride;
                        const auto *components = reinterpret_cast<const u16*>(ptr_offset(binary_chunk->data, offset));
                        new_mesh.positions.push_back({u16_to_float(components[0]), u16_to_float(components[1]), u16_to_float(components[2])});
                    }
                }

                assert(primitive.HasMember("indices"));
                {
                    auto accessor = get_accessor(accessors[primitive["indices"].GetInt()]);
                    new_submesh.index_count = accessor.count;
                    auto bufferview = get_bufferview(bufferviews[accessor.bufferview_index]);

                    usize byte_stride = gltf::size_of(accessor.component_type);

                    new_mesh.indices.reserve(bufferview.byte_length / byte_stride);
                    for (usize i_index = 0; i_index < usize(accessor.count); i_index += 1)
                    {
                        u32 index;
                        usize offset = bufferview.byte_offset + accessor.byte_offset + i_index * byte_stride;
                        if (accessor.component_type == gltf::ComponentType::UnsignedShort) {
                            index = *reinterpret_cast<const u16*>(ptr_offset(binary_chunk->data, offset));
                        }
                        else if (accessor.component_type == gltf::ComponentType::UnsignedInt) {
                            index = *reinterpret_cast<const u32*>(ptr_offset(binary_chunk->data, offset));
                        }
                        else {
                            assert(false);
                        }
                        new_mesh.indices.push_back(index);
                    }

                    index_acc += accessor.count;
                }

#if defined(BUILD_MESHLETS)
                const size_t max_vertices  = 64;
                const size_t max_triangles = 124;  // NVidia-recommended 126, rounded down to a multiple of 4
                const float cone_weight    = 0.5f; // note: should be set to 0 unless cone culling is used at runtime!

                size_t max_meshlets = meshopt_buildMeshletsBound(new_submesh.index_count, max_vertices, max_triangles);
                std::vector<meshopt_Meshlet> meshlets(max_meshlets);
                std::vector<unsigned int> meshlet_vertices(max_meshlets * max_vertices);
                std::vector<unsigned char> meshlet_triangles(max_meshlets * max_triangles * 3);

                meshlets.resize(meshopt_buildMeshlets(&meshlets[0],
                                                      &meshlet_vertices[0],
                                                      &meshlet_triangles[0],
                                                      &new_mesh.indices[new_submesh.first_index],
                                                      new_submesh.index_count,
                                                      &new_mesh.positions[new_submesh.first_vertex].x,
                                                      new_submesh.vertex_count,
                                                      sizeof(float3),
                                                      max_vertices,
                                                      max_triangles,
                                                      cone_weight));

                if (meshlets.size())
                {
                    const meshopt_Meshlet &last = meshlets.back();

                    // this is an example of how to trim the vertex/triangle arrays when copying data out to GPU storage
                    meshlet_vertices.resize(last.vertex_offset + last.vertex_count);
                    meshlet_triangles.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));

                }

                double avg_vertices  = 0;
                double avg_triangles = 0;
                size_t not_full      = 0;

                for (size_t i = 0; i < meshlets.size(); ++i)
                {
                    const meshopt_Meshlet &m = meshlets[i];

                    avg_vertices += m.vertex_count;
                    avg_triangles += m.triangle_count;
                    not_full += m.vertex_count < max_vertices;
                }

                avg_vertices /= double(meshlets.size());
                avg_triangles /= double(meshlets.size());

                logger::info("\t{} meshlets (avg vertices {}, avg triangles {}, not full {})\n",
                             int(meshlets.size()),
                             avg_vertices,
                             avg_triangles,
                             int(not_full));
#endif
            }
        }
    }
}

Scene load_file(const std::string_view &path)
{
    auto file = platform::MappedFile::open(path);
    if (!file)
    {
        return {};
    }

    Scene scene = {};

    scene.file = std::move(*file);

    const auto &header = *reinterpret_cast<const Header*>(scene.file.base_addr);
    if (header.magic != 0x46546C67) {
        logger::error("[GLB] Invalid GLB file.\n");
        return {};
    }

    if (header.first_chunk.type != ChunkType::Json) {
        logger::error("[GLB] First chunk isn't JSON.\n");
        return {};
    }

    std::string_view json_content{reinterpret_cast<const char*>(&header.first_chunk.data), header.first_chunk.length};

    rapidjson::Document document;
    document.Parse(json_content.data(), json_content.size());

    if (document.HasParseError()) {
        logger::error("[GLB] JSON Error at offset {}: {}\n", document.GetErrorOffset(), rapidjson::GetParseError_En(document.GetParseError()));
        return {};
    }

    const Chunk *binary_chunk = nullptr;
    if (sizeof(Header) + header.first_chunk.length < header.length)
    {
        binary_chunk = reinterpret_cast<const Chunk*>(ptr_offset(header.first_chunk.data, header.first_chunk.length));
        if (binary_chunk->type != ChunkType::Binary) {
            logger::error("[GLB] Second chunk isn't BIN.\n");
            return {};
        }
    }

    process_json(scene, document, binary_chunk);

    return scene;
}
}
