#include "gltf.hpp"

#include "base/vector.hpp"
#include "tools.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <future>
#include <simdjson/simdjson.h>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

#define U32(x) static_cast<u32>(x)
#define FLOAT(x) static_cast<float>(x)

namespace gltf
{
struct GltfPrimitiveAttribute
{
    void *data;
    usize len;
};

inline bool json_has(const simdjson::dom::element &object, const char *field_name)
{
    return object[field_name].error() == simdjson::SUCCESS;
}

template <typename T>
inline T json_get_or(const simdjson::dom::element &object, const char *field_name, T default_value)
{
    if (json_has(object, field_name))
    {
        return object[field_name];
    }
    return default_value;
}

static Option<GltfPrimitiveAttribute> gltf_primitive_attribute(Model &model, const simdjson::dom::element &root,
                                                                      const simdjson::dom::element &attributes,
                                                                      const char *attribute)
{
    if (attributes[attribute].error() == simdjson::SUCCESS)
    {
        u32 accessor_i                 = static_cast<u32>(attributes[attribute].get_uint64());
        simdjson::dom::element accessor = root["accessors"].at(accessor_i);
        u32 view_i                     = static_cast<u32>(accessor["bufferView"].get_uint64());
        simdjson::dom::element view                       = root["bufferViews"].at(view_i);
        auto &buffer                    = model.buffers[view["buffer"]];

        usize count       = accessor["count"];
        auto acc_offset  = json_get_or<u64>(accessor, "byteOffset", 0);
        auto view_offset = json_get_or<u64>(view, "byteOffset", 0);
        usize offset      = acc_offset + view_offset;

        GltfPrimitiveAttribute result;
        result.data = reinterpret_cast<void *>(&buffer.data[offset]);
        result.len  = count;

        return std::make_optional(result);
    }
    return std::nullopt;
}

Model load_model(fs::path path)
{
    Model model;
    model.path = path;

    simdjson::dom::parser parser;
    simdjson::dom::element doc = parser.load(path.string());

    // Load buffers file into memory
    for (const auto &json_buffer : doc["buffers"])
    {
        Buffer buf;
        buf.byte_length = json_buffer["byteLength"].get_uint64();

        std::string_view buffer_name = json_buffer["uri"].get_string();
        auto buffer_path = fs::path(path).remove_filename();
        buffer_path += fs::path(buffer_name);
        buf.data                     = tools::read_file(buffer_path);

        model.buffers.push_back(std::move(buf));
    }

    if (json_has(doc, "samplers"))
    {
        for (const auto &json_sampler : doc["samplers"])
        {
            Sampler sampler{};
            if (json_has(json_sampler, "magFilter")) {
                sampler.mag_filter = Filter(static_cast<u32>(json_sampler["magFilter"].get_uint64()));
            }
            if (json_has(json_sampler, "minFilter")) {
                sampler.min_filter = Filter(static_cast<u32>(json_sampler["minFilter"].get_uint64()));
            }
            if (json_has(json_sampler, "wrapS")) {
                sampler.wrap_s     = Wrap(static_cast<u32>(json_sampler["wrapS"].get_uint64()));
            }
            if (json_has(json_sampler, "wrapT")) {
                sampler.wrap_t     = Wrap(static_cast<u32>(json_sampler["wrapT"].get_uint64()));
            }
            model.samplers.push_back(sampler);
        }
    }
    // add a fallback sampler for images without one
    model.samplers.emplace_back();

    assert(!model.samplers.empty());
    if (json_has(doc, "textures"))
    {
        for (const auto &json_texture : doc["textures"])
        {
            Texture texture;
            texture.sampler = json_get_or<u64>(json_texture, "sampler", model.samplers.size() - 1);
            texture.image   = json_texture["source"].get_uint64();
            model.textures.push_back(texture);
        }
    }

    // Load images file into memory
    if (json_has(doc, "images"))
    {
        Vec<std::future<Image>> images_data;
        images_data.resize(doc["images"].get_array().size());

        uint i_image = 0;
        for (const auto &json_image : doc["images"])
        {
            std::string_view image_name = json_image["uri"].get_string();

            auto image_path = fs::path(path).remove_filename();
            image_path += fs::path(image_name);

            images_data[i_image] = std::async(std::launch::async, [=]() {
                Image image;
                image.data = tools::read_file(image_path);
                image.srgb = false;
                return image;
            });

            i_image++;
        }

        model.images.resize(images_data.size());

        for (i_image = 0; i_image < images_data.size(); i_image++)
        {
            model.images[i_image] = images_data[i_image].get();
        }
    }

    if (json_has(doc, "materials"))
    {
    for (const auto &json_material : doc["materials"])
    {
        Material material{};

        if (json_material["emissiveFactor"].error() == simdjson::SUCCESS)
        {
            auto emissive_factor      = json_material["emissiveFactor"];
            material.emissive_factor.r = static_cast<float>(emissive_factor.at(0).get_double());
            material.emissive_factor.g = static_cast<float>(emissive_factor.at(1).get_double());
            material.emissive_factor.b = static_cast<float>(emissive_factor.at(2).get_double());
        }

        if (json_material["pbrMetallicRoughness"].error() == simdjson::SUCCESS)
        {
            const auto &json_pbr         = json_material["pbrMetallicRoughness"];

            if (json_pbr["baseColorFactor"].error() == simdjson::SUCCESS)
            {
                auto base_color_factors      = json_pbr["baseColorFactor"];
                material.base_color_factor.r = static_cast<float>(base_color_factors.at(0).get_double());
                material.base_color_factor.g = static_cast<float>(base_color_factors.at(1).get_double());
                material.base_color_factor.b = static_cast<float>(base_color_factors.at(2).get_double());
                material.base_color_factor.a = static_cast<float>(base_color_factors.at(3).get_double());
            }

            if (json_pbr["metallicFactor"].error() == simdjson::SUCCESS)
            {
                material.metallic_factor = static_cast<float>(json_pbr["metallicFactor"].get_double());
            }
            if (json_pbr["roughnessFactor"].error() == simdjson::SUCCESS)
            {
                material.roughness_factor = static_cast<float>(json_pbr["roughnessFactor"].get_double());
            }

            if (json_pbr["baseColorTexture"].error() == simdjson::SUCCESS)
            {
                u32 i_texture               = U32(json_pbr["baseColorTexture"].at_key("index").get_uint64());
                material.base_color_texture = i_texture;

                auto i_image = model.textures[i_texture].image;
                auto &image  = model.images[i_image];
                image.srgb   = true;
            }

            if (json_pbr["metallicRoughnessTexture"].error() == simdjson::SUCCESS)
            {
                u32 i_texture                       = U32(json_pbr["metallicRoughnessTexture"].at_key("index").get_uint64());
                material.metallic_roughness_texture = i_texture;
            }
        }

        if (json_material["normalTexture"].error() == simdjson::SUCCESS)
        {
            u32 i_texture           = U32(json_material["normalTexture"].at_key("index").get_uint64());
            material.normal_texture = i_texture;
        }

        model.materials.push_back(material);
    }
    }
    // fallback material
    model.materials.emplace_back();

    // Fill the model vertex and index buffers
    for (const auto &json_mesh : doc["meshes"])
    {
        Mesh mesh;
        for (const auto &json_primitive : json_mesh["primitives"])
        {
            Primitive primitive{};
            if (json_has(json_primitive, "material"))
            {
                primitive.material = U32(json_primitive["material"].get_uint64());
            }
            else
            {
                primitive.material = U32(model.materials.size() - 1);
            }

            if (json_has(json_primitive, "mode"))
            {
                primitive.mode = RenderingMode(static_cast<u8>(json_primitive["mode"].get_uint64()));
            }

            const simdjson::dom::element &json_attributes = json_primitive["attributes"];

            primitive.first_vertex = static_cast<u32>(model.vertices.size());
            primitive.first_index  = static_cast<u32>(model.indices.size());

            auto position_attribute = gltf_primitive_attribute(model, doc, json_attributes, "POSITION");
            if (position_attribute)
            {
                auto *positions = reinterpret_cast<float3 *>(position_attribute->data);

                model.vertices.reserve(position_attribute->len);
                primitive.aab_min = positions[0];
                primitive.aab_max = positions[0];
                for (usize i = 0; i < position_attribute->len; i++)
                {
                    Vertex vertex;
                    vertex.position = positions[i];

                    for (uint i_comp = 0; i_comp < 3; i_comp++)
                    {
                        if (vertex.position.raw[i_comp] < primitive.aab_min.raw[i_comp]) {
                            primitive.aab_min.raw[i_comp] = vertex.position.raw[i_comp];
                        }
                        if (vertex.position.raw[i_comp] > primitive.aab_max.raw[i_comp]) {
                            primitive.aab_max.raw[i_comp] = vertex.position.raw[i_comp];
                        }
                    }

                    model.vertices.push_back(vertex);
                }
            }

            auto normal_attribute = gltf_primitive_attribute(model, doc, json_attributes, "NORMAL");
            if (normal_attribute)
            {
                auto *normals = reinterpret_cast<float3 *>(normal_attribute->data);
                for (usize i = 0; i < normal_attribute->len; i++)
                {
                    model.vertices[primitive.first_vertex + i].normal = normals[i];
                }
            }

            auto uv0_attribute = gltf_primitive_attribute(model, doc, json_attributes, "TEXCOORD_0");
            if (uv0_attribute)
            {
                auto *uvs = reinterpret_cast<float2 *>(uv0_attribute->data);
                for (usize i = 0; i < uv0_attribute->len; i++)
                {
                    model.vertices[primitive.first_vertex + i].uv0 = uvs[i];
                }
            }

            auto uv1_attribute = gltf_primitive_attribute(model, doc, json_attributes, "TEXCOORD_1");
            if (uv1_attribute)
            {
                auto *uvs = reinterpret_cast<float2 *>(uv1_attribute->data);
                for (usize i = 0; i < uv1_attribute->len; i++)
                {
                    model.vertices[primitive.first_vertex + i].uv0 = uvs[i];
                }
            }

            auto color0_attribute = gltf_primitive_attribute(model, doc, json_attributes, "COLOR_0");
            if (color0_attribute)
            {
                auto *colors = reinterpret_cast<float4 *>(color0_attribute->data);
                for (usize i = 0; i < color0_attribute->len; i++)
                {
                    model.vertices[primitive.first_vertex + i].color0 = colors[i];
                }
            }

            {
                uint accessor_i                 = U32(json_primitive["indices"].get_uint64());
                simdjson::dom::element accessor = doc["accessors"].at(accessor_i);
                uint view_i                     = U32(accessor["bufferView"].get_uint64());
                auto view                       = doc["bufferViews"].at(view_i);
                auto &buffer                    = model.buffers[view["buffer"]];

                auto component_type = ComponentType(static_cast<u32>(accessor["componentType"].get_uint64()));
                u32 count         = U32(accessor["count"].get_uint64());
                auto acc_offset  = json_get_or<u64>(accessor, "byteOffset", 0);
                usize view_offset = view["byteOffset"];
                usize offset      = acc_offset + view_offset;

                if (component_type == ComponentType::UnsignedShort)
                {
                    auto *indices = reinterpret_cast<u16 *>(&buffer.data[offset]);
                    for (u32 i = 0; i < count; i++)
                    {
                        model.indices.push_back(primitive.first_vertex + indices[i]);
                    }
                }
                else if (component_type == ComponentType::UnsignedByte)
                {
                    auto *indices = reinterpret_cast<u8 *>(&buffer.data[offset]);
                    for (u32 i = 0; i < count; i++)
                    {
                        model.indices.push_back(primitive.first_vertex + indices[i]);
                    }
                }
                else if (component_type == ComponentType::UnsignedInt)
                {
                    auto *indices = reinterpret_cast<u32 *>(&buffer.data[offset]);
                    for (u32 i = 0; i < count; i++)
                    {
                        model.indices.push_back(primitive.first_vertex + indices[i]);
                    }
                }
                else
                {
                    assert(!"Unsupported index format.");
                }

                primitive.index_count = count;
            }

            mesh.primitives.push_back(U32(model.primitives.size()));
            model.primitives.push_back(primitive);
        }
        model.meshes.push_back(std::move(mesh));
    }

    for (const auto &json_node : doc["nodes"])
    {
        Node node{};

        if (json_has(json_node, "mesh"))
        {
            node.mesh = json_node["mesh"];
        }

        if (json_node["matrix"].error() == simdjson::SUCCESS)
        {
            usize i = 0;
            for (double val : json_node["matrix"]) {
                node.transform.at(i%4, i/4) = static_cast<float>(val);
                i += 1;
            }

            assert(i == 16);

        }

        if (json_node["translation"].error() == simdjson::SUCCESS)
        {
            auto translation_factors = json_node["translation"];
            node.translation.x       = static_cast<float>(translation_factors.at(0).get_double());
            node.translation.y       = static_cast<float>(translation_factors.at(1).get_double());
            node.translation.z       = static_cast<float>(translation_factors.at(2).get_double());
        }

        if (json_node["rotation"].error() == simdjson::SUCCESS)
        {
            const auto &rotation = json_node["rotation"];
            node.rotation        = float4(static_cast<float>(rotation.at(0).get_double()),
                                          static_cast<float>(rotation.at(1).get_double()),
                                          static_cast<float>(rotation.at(2).get_double()),
                                          static_cast<float>(rotation.at(3).get_double()));
        }

        if (json_node["scale"].error() == simdjson::SUCCESS)
        {
            auto scale_factors = json_node["scale"];
            node.scale.x       = static_cast<float>(scale_factors.at(0).get_double());
            node.scale.y       = static_cast<float>(scale_factors.at(1).get_double());
            node.scale.z       = static_cast<float>(scale_factors.at(2).get_double());
        }

        if (json_node["children"].error() == simdjson::SUCCESS)
        {
            const auto &children = json_node["children"].get_array();
            node.children.reserve(children.size());
            for (u64 child_i : children)
            {
                node.children.push_back(U32(child_i));
            }
        }

        model.nodes.push_back(std::move(node));
    }

    usize scene_i          = doc["scene"];
    const auto &scene_json = doc["scenes"].at(scene_i);
    for (usize node_i : scene_json["nodes"])
    {
        model.scene.push_back(node_i);
    }

    /// Precompute

    model.cached_transforms.resize(model.nodes.size());

    Vec<u32> nodes_stack;
    nodes_stack.reserve(model.nodes.size());

    Vec<u32> parent_indices;
    parent_indices.reserve(model.nodes.size());

    for (auto scene_root : model.scene)
    {
        nodes_stack.push_back(u32_invalid);
        nodes_stack.push_back(U32(scene_root));
    }

    while (!nodes_stack.empty())
    {
        auto node_idx = nodes_stack.back();
        nodes_stack.pop_back();

        if (node_idx == u32_invalid)
        {
            if (!parent_indices.empty()) {
                parent_indices.pop_back();
            }
            continue;
        }

        auto &node = model.nodes[node_idx];

        // --- preorder
        float constant_scale = 1.0f;

        node.dirty                        = false;
        auto translation                  = float4x4::identity(); // glm::translate(glm::mat4(1.0f), node.translation);
        translation = float4x4({
                1, 0, 0, constant_scale * node.translation.x,
                0, 1, 0, constant_scale * node.translation.y,
                0, 0, 1, constant_scale * node.translation.z,
                0, 0, 0, 1,
            });

        auto rotation                     = float4x4::identity(); // glm::mat4(node.rotation);
        rotation = float4x4({
                1.0f - 2.0f*node.rotation.y*node.rotation.y - 2.0f*node.rotation.z*node.rotation.z, 2.0f*node.rotation.x*node.rotation.y - 2.0f*node.rotation.z*node.rotation.w, 2.0f*node.rotation.x*node.rotation.z + 2.0f*node.rotation.y*node.rotation.w, 0.0f,
                2.0f*node.rotation.x*node.rotation.y + 2.0f*node.rotation.z*node.rotation.w, 1.0f - 2.0f*node.rotation.x*node.rotation.x - 2.0f*node.rotation.z*node.rotation.z, 2.0f*node.rotation.y*node.rotation.z - 2.0f*node.rotation.x*node.rotation.w, 0.0f,
                2.0f*node.rotation.x*node.rotation.z - 2.0f*node.rotation.y*node.rotation.w, 2.0f*node.rotation.y*node.rotation.z + 2.0f*node.rotation.x*node.rotation.w, 1.0f - 2.0f*node.rotation.x*node.rotation.x - 2.0f*node.rotation.y*node.rotation.y, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f,
            });

        auto scale                        = float4x4::identity(); // assume uniform scale
        scale.at(0, 0)                    = constant_scale * node.scale.x;
        scale.at(1, 1)                    = constant_scale * node.scale.y;
        scale.at(2, 2)                    = constant_scale * node.scale.z;

        auto transform = node.transform * translation * rotation * scale;

        auto parent_transform = float4x4::identity();
        if (!parent_indices.empty()) {
            parent_transform = model.cached_transforms[parent_indices.back()];
        }

        model.cached_transforms[node_idx] = parent_transform * transform;

        model.nodes_preorder.push_back(node_idx);

        parent_indices.push_back(node_idx);

        nodes_stack.push_back(u32_invalid);
        // ----

        for (auto child : node.children)
        {
            nodes_stack.push_back(child);
        }
    }

    return model;
}
} // namespace my_app
