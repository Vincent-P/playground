#include "gltf.hpp"
#include "renderer/hl_api.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <future>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // lookAt perspective
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define STB_IMAGE_IMPLEMENTATION
#include "renderer/renderer.hpp"
#include "tools.hpp"

#include <stb_image.h>

namespace my_app
{
using json   = nlohmann::json;
namespace fs = std::filesystem;

struct GltfPrimitiveAttribute
{
    void *data;
    usize len;
};

static std::optional<GltfPrimitiveAttribute> gltf_primitive_attribute(Model &model, const json &root,
                                                                      const json &attributes, const char *attribute)
{
    if (attributes.count(attribute))
    {
        uint accessor_i = attributes[attribute];
        auto accessor   = root["accessors"][accessor_i];
        uint view_i     = accessor["bufferView"];
        auto view       = root["bufferViews"][view_i];
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

Model load_model(const char *c_path)
{
    Model model;
    fs::path path{c_path};

    std::ifstream f{path};
    json j;
    f >> j;

    // Load buffers file into memory
    for (const auto &json_buffer : j["buffers"])
    {
        Buffer buf;
        buf.byte_length = json_buffer["byteLength"];

        const std::string buffer_name = json_buffer["uri"];
        fs::path buffer_path          = path.replace_filename(buffer_name);
        buf.data                      = tools::read_file(buffer_path);

        model.buffers.push_back(std::move(buf));
    }

    // Fill the model vertex and index buffers
    for (const auto &json_mesh : j["meshes"])
    {
        Mesh mesh;
        for (const auto &json_primitive : json_mesh["primitives"])
        {
            Primitive primitive;
            primitive.material = json_primitive["material"];
            primitive.mode     = json_primitive["mode"];

            const auto &json_attributes = json_primitive["attributes"];

            primitive.first_vertex = static_cast<u32>(model.vertices.size());
            primitive.first_index  = static_cast<u32>(model.indices.size());

            auto position_attribute = gltf_primitive_attribute(model, j, json_attributes, "POSITION");
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

            auto normal_attribute = gltf_primitive_attribute(model, j, json_attributes, "NORMAL");
            if (normal_attribute)
            {
                auto *normals = reinterpret_cast<float3 *>(normal_attribute->data);
                for (usize i = 0; i < normal_attribute->len; i++)
                {
                    model.vertices[primitive.first_vertex + i].normal = normals[i];
                }
            }

            auto uv0_attribute = gltf_primitive_attribute(model, j, json_attributes, "TEXCOORD_0");
            if (uv0_attribute)
            {
                auto *uvs = reinterpret_cast<float2 *>(uv0_attribute->data);
                for (usize i = 0; i < uv0_attribute->len; i++)
                {
                    model.vertices[primitive.first_vertex + i].uv0 = uvs[i];
                }
            }

            auto uv1_attribute = gltf_primitive_attribute(model, j, json_attributes, "TEXCOORD_1");
            if (uv1_attribute)
            {
                auto *uvs = reinterpret_cast<float2 *>(uv1_attribute->data);
                for (usize i = 0; i < uv1_attribute->len; i++)
                {
                    model.vertices[primitive.first_vertex + i].uv0 = uvs[i];
                }
            }

            {
                uint accessor_i = json_primitive["indices"];
                auto accessor   = j["accessors"][accessor_i];
                uint view_i     = accessor["bufferView"];
                auto view       = j["bufferViews"][view_i];
                auto &buffer    = model.buffers[view["buffer"]];

                u32 count         = accessor["count"];
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

    for (const auto &json_node : j["nodes"])
    {
        Node node;

        node.mesh = json_node["mesh"];

        if (json_node.count("matrix"))
        {
        }

        if (json_node.count("translation"))
        {
            auto translation_factors = json_node["translation"];
            node.translation.x       = translation_factors[0];
            node.translation.y       = translation_factors[1];
            node.translation.z       = translation_factors[2];
        }

        if (json_node.count("rotation"))
        {
            const auto &rotation = json_node["rotation"];
            node.rotation        = glm::quat(rotation[0], rotation[1], rotation[2], rotation[3]);
        }

        if (json_node.count("scale"))
        {
            auto scale_factors = json_node["scale"];
            node.scale.x       = scale_factors[0];
            node.scale.y       = scale_factors[1];
            node.scale.z       = scale_factors[2];
        }

        if (json_node.count("children"))
        {
            const auto &children = json_node["children"];
            node.children.reserve(children.size());
            for (u32 child_i : children)
            {
                node.children.push_back(child_i);
            }
        }

        model.nodes.push_back(std::move(node));
    }

    for (const auto &json_sampler : j["samplers"])
    {
        Sampler sampler;
        sampler.mag_filter = json_sampler["magFilter"];
        sampler.min_filter = json_sampler["minFilter"];
        sampler.wrap_s     = json_sampler["wrapS"];
        sampler.wrap_t     = json_sampler["wrapT"];
        model.samplers.push_back(std::move(sampler));
    }

    // Load images file into memory
    std::vector<std::future<Image>> images_data;
    images_data.resize(j["images"].size());

    for (uint i = 0; i < images_data.size(); i++)
    {
        const auto &json_image = j["images"][i];
        std::string type       = json_image["mimeType"];
        std::string image_name = json_image["uri"];
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
    }

    model.images.resize(images_data.size());
    for (uint i = 0; i < images_data.size(); i++)
    {
        model.images[i] = images_data[i].get();
    }

    for (const auto &json_texture : j["textures"])
    {
        Texture texture;
        texture.sampler = json_texture["sampler"];
        texture.image   = json_texture["source"];
        model.textures.push_back(std::move(texture));
    }

    for (const auto &json_material : j["materials"])
    {
        Material material;

        if (json_material.count("pbrMetallicRoughness"))
        {
            const auto &json_pbr         = json_material["pbrMetallicRoughness"];
            auto base_color_factors      = json_pbr["baseColorFactor"];
            material.base_color_factor.r = base_color_factors[0];
            material.base_color_factor.g = base_color_factors[1];
            material.base_color_factor.b = base_color_factors[2];
            material.base_color_factor.a = base_color_factors[3];

            if (json_pbr.count("metallicFactor")) {
                material.metallic_factor  = json_pbr["metallicFactor"];
            }
            if (json_pbr.count("roughnessFactor")) {
                material.roughness_factor = json_pbr["roughnessFactor"];
            }

            if (json_pbr.count("baseColorTexture"))
            {
                u32 i_texture               = json_pbr["baseColorTexture"]["index"];
                material.base_color_texture = i_texture;

                auto i_image = model.textures[i_texture].image;
                auto &image  = model.images[i_image];
                image.srgb   = true;
            }

            if (json_pbr.count("metallicRoughnessTexture"))
            {
                u32 i_texture                       = json_pbr["metallicRoughnessTexture"]["index"];
                material.metallic_roughness_texture = i_texture;
            }
        }

        if (json_material.count("normalTexture"))
        {
            u32 i_texture           = json_material["normalTexture"]["index"];
            material.normal_texture = i_texture;
        }

        model.materials.push_back(std::move(material));
    }

    usize scene_i          = j["scene"];
    const auto &scene_json = j["scenes"][scene_i];
    for (usize node_i : scene_json["nodes"])
    {
        model.scene.push_back(node_i);
    }

    return model;
}
} // namespace my_app
