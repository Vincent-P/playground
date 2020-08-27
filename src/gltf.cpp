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

MaterialPushConstant MaterialPushConstant::from(const Material &material)
{
    MaterialPushConstant result;
    result.base_color_factor = material.base_color_factor;
    return result;
}

void Renderer::load_model_data()
{
    {
        usize buffer_size = model.vertices.size() * sizeof(GltfVertex);

        vulkan::BufferInfo info;
        info.name           = "glTF Vertex Buffer";
        info.size           = buffer_size;
        info.usage          = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        info.memory_usage   = VMA_MEMORY_USAGE_GPU_ONLY;
        model.vertex_buffer = api.create_buffer(info);
        api.upload_buffer(model.vertex_buffer, model.vertices.data(), buffer_size);
    }

    {
        usize buffer_size = model.indices.size() * sizeof(u16);

        vulkan::BufferInfo info;
        info.name          = "glTF Index Buffer";
        info.size          = buffer_size;
        info.usage         = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        info.memory_usage  = VMA_MEMORY_USAGE_GPU_ONLY;
        model.index_buffer = api.create_buffer(info);
        api.upload_buffer(model.index_buffer, model.indices.data(), buffer_size);
    }

    // send images data to gpu
    {
        for (auto &sampler : model.samplers)
        {
            vulkan::SamplerInfo sinfo;

            switch (sampler.mag_filter)
            {
                case Filter::Nearest:
                    sinfo.mag_filter = VK_FILTER_NEAREST;
                    break;
                case Filter::Linear:
                    sinfo.mag_filter = VK_FILTER_LINEAR;
                    break;
                default:
                    break;
            }

            switch (sampler.min_filter)
            {
                case Filter::Nearest:
                    sinfo.min_filter = VK_FILTER_NEAREST;
                    break;
                case Filter::Linear:
                    sinfo.min_filter = VK_FILTER_LINEAR;
                    break;
                default:
                    break;
            }

            switch (sampler.wrap_s)
            {
                case Wrap::Repeat:
                    sinfo.address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                    break;
                case Wrap::ClampToEdge:
                    sinfo.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
                    break;
                case Wrap::MirroredRepeat:
                    sinfo.address_mode = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
                    break;
                default:
                    break;
            }

            sampler.sampler_h = api.create_sampler(sinfo);
        }

        struct ImageInfo
        {
            int width;
            int height;
            u8 *pixels;
            int nb_comp;
            VkFormat format;
        };
        std::vector<std::future<ImageInfo>> images_pixels;
        images_pixels.resize(model.images.size());

        for (usize image_i = 0; image_i < model.images.size(); image_i++)
        {
            const auto &image      = model.images[image_i];
            images_pixels[image_i] = std::async(std::launch::async, [&]() {
                ImageInfo info;
                info.width  = 0;
                info.height = 0;

                info.nb_comp = 0;
                info.pixels  = stbi_load_from_memory(image.data.data(),
                                                    static_cast<int>(image.data.size()),
                                                    &info.width,
                                                    &info.height,
                                                    &info.nb_comp,
                                                    0);

                if (info.nb_comp == 1)
                { // NOLINT
                    info.format = VK_FORMAT_R8_UNORM;
                }
                else if (info.nb_comp == 2)
                { // NOLINT
                    info.format = VK_FORMAT_R8G8_UNORM;
                }
                else if (info.nb_comp == 3)
                { // NOLINT
                    stbi_image_free(info.pixels);
                    int wanted_nb_comp = 4;
                    info.pixels        = stbi_load_from_memory(image.data.data(),
                                                        static_cast<int>(image.data.size()),
                                                        &info.width,
                                                        &info.height,
                                                        &info.nb_comp,
                                                        wanted_nb_comp);
                    info.format        = image.srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
                    info.nb_comp       = wanted_nb_comp;
                }
                else if (info.nb_comp == 4)
                { // NOLINT
                    info.format = image.srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
                }
                else
                { // NOLINT
                    assert(false);
                }

                return info;
            });
        }

        std::vector<VkImageMemoryBarrier> barriers;
        VkPipelineStageFlags src_stage = 0;
        VkPipelineStageFlags dst_stage = 0;
        for (usize image_i = 0; image_i < model.images.size(); image_i++)
        {
            auto &image     = model.images[image_i];
            auto image_info = images_pixels[image_i].get();

            vulkan::ImageInfo iinfo;
            iinfo.name                = "glTF image";
            iinfo.width               = static_cast<u32>(image_info.width);
            iinfo.height              = static_cast<u32>(image_info.height);
            iinfo.depth               = 1;
            iinfo.format              = image_info.format;
            iinfo.generate_mip_levels = true;
            image.image_h             = api.create_image(iinfo);
            auto size                 = static_cast<usize>(image_info.width * image_info.height * image_info.nb_comp);
            api.upload_image(image.image_h, image_info.pixels, size);
            api.generate_mipmaps(image.image_h);

            barriers.emplace_back();
            auto &b = barriers.back();

            auto &vkimage = api.get_image(image.image_h);
            auto src      = vulkan::get_src_image_access(vkimage.usage);
            auto dst      = vulkan::get_dst_image_access(vulkan::ImageUsage::GraphicsShaderRead);

            b                  = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            b.oldLayout        = src.layout;
            b.newLayout        = dst.layout;
            b.srcAccessMask    = src.access;
            b.dstAccessMask    = dst.access;
            b.image            = vkimage.vkhandle;
            b.subresourceRange = vkimage.full_range;
            vkimage.usage      = vulkan::ImageUsage::GraphicsShaderRead;

            src_stage |= src.stage;
            dst_stage |= dst.stage;

            stbi_image_free(image_info.pixels);
        }

        auto cmd_buffer = api.get_temp_cmd_buffer();
        cmd_buffer.begin();
        vkCmdPipelineBarrier(cmd_buffer.vkhandle,
                             src_stage,
                             dst_stage,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             barriers.size(),
                             barriers.data());
        cmd_buffer.submit_and_wait();
    }

    vulkan::GraphicsProgramInfo pinfo{};
    pinfo.vertex_shader   = api.create_shader("shaders/gltf.vert.spv");
    pinfo.fragment_shader = api.create_shader("shaders/gltf.frag.spv");

    pinfo.push_constant({.stages = VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = sizeof(MaterialPushConstant)});

    // camera uniform buffer
    pinfo.binding({.set    = vulkan::GLOBAL_DESCRIPTOR_SET,
                   .slot   = 0,
                   .stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
                   .type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                   .count  = 1});

    // debug shader output
    pinfo.binding({.set    = vulkan::SHADER_DESCRIPTOR_SET,
                   .slot   = 0,
                   .stages = VK_SHADER_STAGE_FRAGMENT_BIT,
                   .type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                   .count  = 1});

    pinfo.binding({.set    = vulkan::SHADER_DESCRIPTOR_SET,
                   .slot   = 1,
                   .stages = VK_SHADER_STAGE_FRAGMENT_BIT,
                   .type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                   .count  = 1});

    pinfo.binding({.set    = vulkan::SHADER_DESCRIPTOR_SET,
                   .slot   = 2,
                   .stages = VK_SHADER_STAGE_FRAGMENT_BIT,
                   .type   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   .count  = 1});

    pinfo.binding({.set    = vulkan::SHADER_DESCRIPTOR_SET,
                   .slot   = 3,
                   .stages = VK_SHADER_STAGE_FRAGMENT_BIT,
                   .type   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   .count  = 6});

    // node transform
    pinfo.binding({.set    = vulkan::DRAW_DESCRIPTOR_SET,
                   .slot   = 0,
                   .stages = VK_SHADER_STAGE_VERTEX_BIT,
                   .type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                   .count  = 1});
    // base color texture
    pinfo.binding({.set    = vulkan::DRAW_DESCRIPTOR_SET,
                   .slot   = 1,
                   .stages = VK_SHADER_STAGE_FRAGMENT_BIT,
                   .type   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   .count  = 1});
    // normal map texture
    pinfo.binding({.set    = vulkan::DRAW_DESCRIPTOR_SET,
                   .slot   = 2,
                   .stages = VK_SHADER_STAGE_FRAGMENT_BIT,
                   .type   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   .count  = 1});
    // metallic roughness texture
    pinfo.binding({.set    = vulkan::DRAW_DESCRIPTOR_SET,
                   .slot   = 3,
                   .stages = VK_SHADER_STAGE_FRAGMENT_BIT,
                   .type   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   .count  = 1});

    pinfo.vertex_stride(sizeof(GltfVertex));
    pinfo.vertex_info({.format = VK_FORMAT_R32G32B32_SFLOAT, .offset = MEMBER_OFFSET(GltfVertex, position)});
    pinfo.vertex_info({.format = VK_FORMAT_R32G32B32_SFLOAT, .offset = MEMBER_OFFSET(GltfVertex, normal)});
    pinfo.vertex_info({.format = VK_FORMAT_R32G32_SFLOAT, .offset = MEMBER_OFFSET(GltfVertex, uv0)});
    pinfo.vertex_info({.format = VK_FORMAT_R32G32_SFLOAT, .offset = MEMBER_OFFSET(GltfVertex, uv1)});
    pinfo.vertex_info({.format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = MEMBER_OFFSET(GltfVertex, joint0)});
    pinfo.vertex_info({.format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = MEMBER_OFFSET(GltfVertex, weight0)});
    pinfo.depth_test         = VK_COMPARE_OP_EQUAL; // equal because depth prepass
    pinfo.enable_depth_write = false;

    model.program = api.create_program(std::move(pinfo));
}

void Renderer::destroy_model()
{
    for (auto &image : model.images)
    {
        api.destroy_image(image.image_h);
    }
    api.destroy_buffer(model.vertex_buffer);
    api.destroy_buffer(model.index_buffer);
}

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
