#include "gltf.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "tools.hpp"
#include "renderer/renderer.hpp"

namespace my_app
{
using json   = nlohmann::json;
namespace fs = std::filesystem;

void Renderer::draw_model()
{
    vk::Viewport viewport{};
    viewport.width    = api.ctx.swapchain.extent.width;
    viewport.height   = api.ctx.swapchain.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    api.set_viewport(viewport);

    vk::Rect2D scissor{};
    scissor.extent = api.ctx.swapchain.extent;
    api.set_scissor(scissor);

    // last program
    // bind program
    api.bind_program(model.program);

    api.bind_index_buffer(model.index_buffer);
    api.bind_vertex_buffer(model.vertex_buffer);

    for (const auto &mesh : model.meshes) {
        for (const auto& primitive : mesh.primitives) {
            // if program != last program then bind program
            api.draw_indexed(primitive.index_count, 1, primitive.first_index, static_cast<i32>(primitive.first_vertex), 0);
        }
    }
}

void Renderer::load_model_data()
{
    {
        usize              buffer_size = model.vertices.size() * sizeof(GltfVertex);

        vulkan::BufferInfo info;
        info.name = "glTF Vertex Buffer";
        info.size = buffer_size;
        info.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
        info.memory_usage = VMA_MEMORY_USAGE_GPU_ONLY;
        model.vertex_buffer = api.create_buffer(info);
        api.upload_buffer(model.vertex_buffer, model.vertices.data(), buffer_size);
    }

    {
        usize              buffer_size = model.indices.size() * sizeof(u16);

        vulkan::BufferInfo info;
        info.name = "glTF Index Buffer";
        info.size = buffer_size;
        info.usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;
        info.memory_usage = VMA_MEMORY_USAGE_GPU_ONLY;
        model.index_buffer = api.create_buffer(info);
        api.upload_buffer(model.index_buffer, model.indices.data(), buffer_size);
    }

    vulkan::ProgramInfo pinfo{};
    pinfo.vertex_shader = api.create_shader("shaders/gltf.vert.spv");
    pinfo.fragment_shader = api.create_shader("shaders/gltf.frag.spv");

    pinfo.vertex_stride(sizeof(GltfVertex));
    pinfo.vertex_info({vk::Format::eR32G32B32Sfloat, MEMBER_OFFSET(GltfVertex, position)});
    pinfo.vertex_info({vk::Format::eR32G32B32Sfloat, MEMBER_OFFSET(GltfVertex, normal)});
    pinfo.vertex_info({vk::Format::eR32G32Sfloat, MEMBER_OFFSET(GltfVertex, uv0)});
    pinfo.vertex_info({vk::Format::eR32G32Sfloat, MEMBER_OFFSET(GltfVertex, uv1)});
    pinfo.vertex_info({vk::Format::eR32G32B32A32Sfloat, MEMBER_OFFSET(GltfVertex, joint0)});
    pinfo.vertex_info({vk::Format::eR32G32B32A32Sfloat, MEMBER_OFFSET(GltfVertex, weight0)});

    model.program = api.create_program(std::move(pinfo));
}


struct GltfPrimitiveAttribute
{
    void* data;
    usize len;
};

static std::optional<GltfPrimitiveAttribute> gltf_primitive_attribute(Model& model, const json& root, const json& attributes, const char* attribute)
{
    if (attributes.count(attribute))
    {
        uint accessor_i = attributes[attribute];
        auto accessor = root["accessors"][accessor_i];
        uint view_i = accessor["bufferView"];
        auto view = root["bufferViews"][view_i];
        auto &buffer = model.buffers[view["buffer"]];

        usize count = accessor["count"];
        usize acc_offset = accessor["byteOffset"];
        usize view_offset = view["byteOffset"];
        usize offset = acc_offset + view_offset;

        GltfPrimitiveAttribute result;
        result.data = reinterpret_cast<void*>(&buffer.data[offset]);
        result.len = count;

        return std::make_optional(result);
    }
    return std::nullopt;
}

Model load_model(const char *path)
{
    fs::path gltf_path{path};

    Model model;

    std::ifstream f{path};
    json j;
    f >> j;

    // Load buffers file into memory
    for (const auto &json_buffer : j["buffers"]) {
        Buffer buf;
        buf.byte_length = json_buffer["byteLength"];

        const std::string buffer_name = json_buffer["uri"];
        fs::path buffer_path = gltf_path.replace_filename(buffer_name);
        buf.data = tools::read_file(buffer_path);

        model.buffers.push_back(std::move(buf));
    }

    // Fill the model vertex and index buffers
    for (const auto &json_mesh : j["meshes"]) {
        Mesh mesh;
        for (const auto &json_primitive : json_mesh["primitives"]) {
            Primitive primitive;
            primitive.material         = json_primitive["material"];
            primitive.mode = json_primitive["mode"];

            const auto &json_attributes = json_primitive["attributes"];

            primitive.first_vertex = static_cast<u32>(model.vertices.size());
            primitive.first_index = static_cast<u32>(model.indices.size());

            auto position_attribute = gltf_primitive_attribute(model, j, json_attributes, "POSITION");
            {
                auto *positions = reinterpret_cast<float3 *>(position_attribute->data);

                model.vertices.reserve(position_attribute->len);
                for (usize i = 0; i < position_attribute->len; i++) {
                    GltfVertex vertex;
                    vertex.position = positions[i];
                    model.vertices.push_back(std::move(vertex));
                }
            }

            auto normal_attribute = gltf_primitive_attribute(model, j, json_attributes, "NORMAL");
            if (normal_attribute)
            {
                auto *normals = reinterpret_cast<float3*>(normal_attribute->data);
                for (usize i = 0; i < normal_attribute->len; i++) {
                    model.vertices[primitive.first_vertex + i].normal = normals[i];
                }
            }

            auto uv0_attribute = gltf_primitive_attribute(model, j, json_attributes, "TEXCOORD_0");
            if (uv0_attribute)
            {
                auto *uvs = reinterpret_cast<float2*>(uv0_attribute->data);
                for (usize i = 0; i < uv0_attribute->len; i++) {
                    model.vertices[primitive.first_vertex + i].uv0 = uvs[i];
                }
            }

            auto uv1_attribute = gltf_primitive_attribute(model, j, json_attributes, "TEXCOORD_1");
            if (uv1_attribute)
            {
                auto *uvs = reinterpret_cast<float2*>(uv1_attribute->data);
                for (usize i = 0; i < uv1_attribute->len; i++) {
                    model.vertices[primitive.first_vertex + i].uv0 = uvs[i];
                }
            }

            {
                uint accessor_i = json_primitive["indices"];
                auto accessor = j["accessors"][accessor_i];
                uint view_i = accessor["bufferView"];
                auto view = j["bufferViews"][view_i];
                auto &buffer = model.buffers[view["buffer"]];

                usize count = accessor["count"];
                usize acc_offset = accessor["byteOffset"];
                usize view_offset = view["byteOffset"];
                usize offset = acc_offset + view_offset;

                auto *indices = reinterpret_cast<u16*>(&buffer.data[offset]);
                for (usize i = 0; i < count; i++) {
                    model.indices.push_back(indices[i]);
                }

                primitive.index_count = count;
            }

            mesh.primitives.push_back(std::move(primitive));
        }
        model.meshes.push_back(std::move(mesh));
    }

    return model;
}
} // namespace my_app
