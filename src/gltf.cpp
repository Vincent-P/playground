#include "gltf.hpp"

#include "renderer/hl_api.hpp"
#include "renderer/renderer.hpp"
#include "tools.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <simdjson/simdjson.h>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace my_app
{
namespace fs = std::filesystem;

struct GltfPrimitiveAttribute
{
    void *data;
    usize len;
};

static std::optional<GltfPrimitiveAttribute> gltf_primitive_attribute(Model &model, const simdjson::dom::element &root,
                                                                      const simdjson::dom::element &attributes,
                                                                      const char *attribute)
{
    if (attributes[attribute].error() == simdjson::SUCCESS)
    {
        uint accessor_i = attributes[attribute].get_uint64();
        auto accessor   = root["accessors"].at(accessor_i);
        uint view_i     = accessor["bufferView"].get_uint64();
        auto view       = root["bufferViews"].at(view_i);
        auto &buffer    = model.buffers[view["buffer"]];

        usize count       = accessor["count"];
        usize acc_offset  = accessor["byteOffset"];
        usize view_offset = view["byteOffset"];
        usize offset      = acc_offset + view_offset;

        GltfPrimitiveAttribute result;
        result.data = reinterpret_cast<void *>(&buffer.data[offset]);
        result.len  = count;

        return std::make_optional(result);
    }
    return std::nullopt;
}

Model load_model(std::string_view path_view)
{
    fs::path path(path_view);
    Model model;

    simdjson::dom::parser parser;
    simdjson::dom::element doc = parser.load(std::string(path_view));

    // Load buffers file into memory
    for (const auto &json_buffer : doc["buffers"])
    {
        Buffer buf;
        buf.byte_length = json_buffer["byteLength"].get_uint64();

        std::string_view buffer_name = json_buffer["uri"].get_string();
        fs::path buffer_path          = path.replace_filename(fs::path(buffer_name));
        buf.data                      = tools::read_file(buffer_path);

        model.buffers.push_back(std::move(buf));
    }

    // Fill the model vertex and index buffers
    for (const auto &json_mesh : doc["meshes"])
    {
        Mesh mesh;
        for (const auto &json_primitive : json_mesh["primitives"])
        {
            Primitive primitive;
            primitive.material = json_primitive["material"].get_uint64();
            primitive.mode     = RenderingMode(static_cast<u8>(json_primitive["mode"].get_uint64()));

            const simdjson::dom::element &json_attributes = json_primitive["attributes"];

            primitive.first_vertex = static_cast<u32>(model.vertices.size());
            primitive.first_index  = static_cast<u32>(model.indices.size());

            auto position_attribute = gltf_primitive_attribute(model, doc, json_attributes, "POSITION");
            {
                auto *positions = reinterpret_cast<float3 *>(position_attribute->data);

                model.vertices.reserve(position_attribute->len);
                for (usize i = 0; i < position_attribute->len; i++)
                {
                    GltfVertex vertex;
                    vertex.position = positions[i];
                    model.vertices.push_back(std::move(vertex));
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

            {
                uint accessor_i = json_primitive["indices"].get_uint64();
                auto accessor   = doc["accessors"].at(accessor_i);
                uint view_i     = accessor["bufferView"].get_uint64();
                auto view       = doc["bufferViews"].at(view_i);
                auto &buffer    = model.buffers[view["buffer"]];

                u32 count         = accessor["count"].get_uint64();
                usize acc_offset  = accessor["byteOffset"];
                usize view_offset = view["byteOffset"];
                usize offset      = acc_offset + view_offset;

                auto *indices = reinterpret_cast<u16 *>(&buffer.data[offset]);
                for (u32 i = 0; i < count; i++)
                {
                    model.indices.push_back(indices[i]);
                }

                primitive.index_count = count;
            }

            mesh.primitives.push_back(std::move(primitive));
        }
        model.meshes.push_back(std::move(mesh));
    }

    for (const auto &json_node : doc["nodes"])
    {
        Node node;

        node.mesh = json_node["mesh"];

        if (json_node["matrix"].error() == simdjson::SUCCESS)
        {
        }

        if (json_node["translation"].error() == simdjson::SUCCESS)
        {
            auto translation_factors = json_node["translation"];
            node.translation.x       = translation_factors.at(0).get_double();
            node.translation.y       = translation_factors.at(1).get_double();
            node.translation.z       = translation_factors.at(2).get_double();
        }

        if (json_node["rotation"].error() == simdjson::SUCCESS)
        {
            const auto &rotation = json_node["rotation"];
            node.rotation        = float4(
                rotation.at(0).get_double(),
                rotation.at(1).get_double(),
                rotation.at(2).get_double(),
                rotation.at(3).get_double()
            );
        }

        if (json_node["scale"].error() == simdjson::SUCCESS)
        {
            auto scale_factors = json_node["scale"];
            node.scale.x       = scale_factors.at(0).get_double();
            node.scale.y       = scale_factors.at(1).get_double();
            node.scale.z       = scale_factors.at(2).get_double();
        }

        if (json_node["children"].error() == simdjson::SUCCESS)
        {
            const auto &children = json_node["children"].get_array();
            node.children.reserve(children.size());
            for (u64 child_i : children)
            {
                node.children.push_back(child_i);
            }
        }

        model.nodes.push_back(std::move(node));
    }

    for (const auto &json_sampler : doc["samplers"])
    {
        Sampler sampler;
        sampler.mag_filter = Filter(static_cast<u32>(json_sampler["magFilter"].get_uint64()));
        sampler.min_filter = Filter(static_cast<u32>(json_sampler["minFilter"].get_uint64()));
        sampler.wrap_s     = Wrap(static_cast<u32>(json_sampler["wrapS"].get_uint64()));
        sampler.wrap_t     = Wrap(static_cast<u32>(json_sampler["wrapT"].get_uint64()));
        model.samplers.push_back(std::move(sampler));
    }

    // Load images file into memory
    std::vector<std::future<Image>> images_data;
    images_data.resize(doc["images"].get_array().size());

    uint i = 0;
    for (const auto& json_image : doc["images"])
    {
        std::string_view type_view       = json_image["mimeType"].get_string();
        std::string type(type_view);
        std::string_view image_name = json_image["uri"].get_string();
        fs::path image_path    = path.replace_filename(image_name);

        if (type == "image/jpeg")
        {
        }
        else if (type == "image/png")
        {
        }
        else
        {
            throw std::runtime_error("unsupported image type: " + type);
        }

        images_data[i] = std::async(std::launch::async, [=]() {
            Image image;
            image.data = tools::read_file(image_path);
            image.srgb = false;
            return image;
        });

        i++;
    }

    model.images.resize(images_data.size());
    for (uint i = 0; i < images_data.size(); i++)
    {
        model.images[i] = images_data[i].get();
    }

    for (const auto &json_texture : doc["textures"])
    {
        Texture texture;
        texture.sampler = json_texture["sampler"].get_uint64();
        texture.image   = json_texture["source"].get_uint64();
        model.textures.push_back(std::move(texture));
    }

    for (const auto &json_material : doc["materials"])
    {
        Material material;

        if (json_material["pbrMetallicRoughness"].error() == simdjson::SUCCESS)
        {
            const auto &json_pbr         = json_material["pbrMetallicRoughness"];
            auto base_color_factors      = json_pbr["baseColorFactor"];
            material.base_color_factor.r = base_color_factors.at(0).get_double();
            material.base_color_factor.g = base_color_factors.at(1).get_double();
            material.base_color_factor.b = base_color_factors.at(2).get_double();
            material.base_color_factor.a = base_color_factors.at(3).get_double();

            if (json_pbr["metallicFactor"].error() == simdjson::SUCCESS)
            {
                material.metallic_factor  = json_pbr["metallicFactor"].get_double();
            }
            if (json_pbr["roughnessFactor"].error() == simdjson::SUCCESS)
            {
                material.roughness_factor = json_pbr["roughnessFactor"].get_double();
            }

            if (json_pbr["baseColorTexture"].error() == simdjson::SUCCESS)
            {
                u32 i_texture               = json_pbr["baseColorTexture"].at_key("index").get_uint64();
                material.base_color_texture = i_texture;

                auto i_image = model.textures[i_texture].image;
                auto &image  = model.images[i_image];
                image.srgb   = true;
            }

            if (json_pbr["metallicRoughnessTexture"].error() == simdjson::SUCCESS)
            {
                u32 i_texture                       = json_pbr["metallicRoughnessTexture"].at_key("index").get_uint64();
                material.metallic_roughness_texture = i_texture;
            }
        }

        if (json_material["normalTexture"].error() == simdjson::SUCCESS)
        {
            u32 i_texture           = json_material["normalTexture"].at_key("index").get_uint64();
            material.normal_texture = i_texture;
        }

        model.materials.push_back(std::move(material));
    }

    usize scene_i          = doc["scene"];
    const auto &scene_json = doc["scenes"].at(scene_i);
    for (usize node_i : scene_json["nodes"])
    {
        model.scene.push_back(node_i);
    }

    return model;
}
} // namespace my_app
