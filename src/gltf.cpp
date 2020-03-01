#include "gltf.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp> // lookAt perspective

#include <intrin.h>

#define STBI_NO_SIMD // doesnt compile on windows
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "tools.hpp"
#include "renderer/renderer.hpp"

namespace my_app
{
using json   = nlohmann::json;
namespace fs = std::filesystem;

MaterialPushConstant MaterialPushConstant::from(const Material& material)
{
    MaterialPushConstant result;
    result.base_color_factor = material.base_color_factor;
    return result;
}

static void draw_node(Renderer& r, Node& node)
{
    if (node.dirty) {
        node.dirty = false;
        auto translation = glm::translate(glm::mat4(1.0f), node.translation);
        auto rotation = glm::mat4(node.rotation);
        auto scale = glm::scale(glm::mat4(1.0f), node.scale);
        node.cached_transform =  translation * rotation * scale;
    }

    auto u_pos = r.api.dynamic_uniform_buffer(sizeof(float4x4));
    auto* buffer = reinterpret_cast<float4x4*>(u_pos.mapped);
    *buffer = node.cached_transform;
    r.api.bind_buffer(r.model.program, vulkan::DRAW_DESCRIPTOR_SET, 0, u_pos);

    const auto& mesh = r.model.meshes[node.mesh];
    for (const auto& primitive : mesh.primitives) {
        // if program != last program then bind program

        const auto& material = r.model.materials[primitive.material];

        MaterialPushConstant material_pc = MaterialPushConstant::from(material);
        r.api.push_constant(vk::ShaderStageFlagBits::eFragment, 0, sizeof(material_pc), &material_pc);

        if (material.base_color_texture) {
            auto base_color_texture = r.model.textures[*material.base_color_texture];
            auto base_color_image = r.model.images[base_color_texture.image];
            r.api.bind_image(r.model.program, vulkan::DRAW_DESCRIPTOR_SET, 1, base_color_image.image_h);
        }
        else {
            // bind empty texture
        }

        r.api.draw_indexed(primitive.index_count, 1, primitive.first_index, static_cast<i32>(primitive.first_vertex), 0);
    }

    for (auto child_i : node.children) {
        draw_node(r, r.model.nodes[child_i]);
    }
}

void Renderer::draw_model()
{
    float3 cam_up = float3(0, 1, 0);


    ImGui::Begin("Camera");
    static float s_camera_pos[3] = {2.0f, 0.5f, 0.5f};
    ImGui::SliderFloat3("Camera position", s_camera_pos, -25.0f, 25.0f);
    static float s_target_pos[3] = {1.0f, 0.0f, 0.0f};
    ImGui::SliderFloat3("Camera target", s_target_pos, -1.0f, 1.0f);

    float3 target_pos;
    target_pos.x = s_target_pos[0];
    target_pos.y = s_target_pos[1];
    target_pos.z = s_target_pos[2];

    float3 camera_pos;
    camera_pos.x = s_camera_pos[0];
    camera_pos.y = s_camera_pos[1];
    camera_pos.z = s_camera_pos[2];

    float aspect_ratio = api.ctx.swapchain.extent.width / float(api.ctx.swapchain.extent.height);

    static float fov          = 45.0f;
    ImGui::SliderFloat("FOV", &fov, 30.f, 90.f);

    float4x4 proj      = glm::perspective(glm::radians(fov), aspect_ratio, 0.1f, 5000.0f);

    ImGui::SetCursorPosX(10.0f);
    ImGui::Text("Target position:");
    ImGui::SetCursorPosX(20.0f);
    ImGui::Text("X: %.1f", double(target_pos.x));
    ImGui::SetCursorPosX(20.0f);
    ImGui::Text("Y: %.1f", double(target_pos.y));
    ImGui::SetCursorPosX(20.0f);
    ImGui::Text("Z: %.1f", double(target_pos.z));

    // clang-format off
    float4x4 view = glm::lookAt(
	camera_pos,       // origin of camera
	camera_pos + target_pos,    // where to look
	cam_up);

    // Vulkan clip space has inverted Y and half Z.
    float4x4 clip = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f,
			 0.0f, -1.0f, 0.0f, 0.0f,
			 0.0f, 0.0f, 0.5f, 0.0f,
			 0.0f, 0.0f, 0.5f, 1.0f);

    // clang-format on
    ImGui::End();

    auto u_pos = api.dynamic_uniform_buffer(3 * sizeof(float4x4));
    auto* buffer = reinterpret_cast<float4x4*>(u_pos.mapped);
    buffer[0] = view;
    buffer[1] = proj;
    buffer[2] = clip;

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
    api.bind_buffer(model.program, vulkan::SHADER_DESCRIPTOR_SET, 0, u_pos);
    api.bind_program(model.program);
    api.bind_index_buffer(model.index_buffer);
    api.bind_vertex_buffer(model.vertex_buffer);

    for (usize node_i : model.scene) {
        draw_node(*this, model.nodes[node_i]);
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

    // send images data to gpu
    {
        for (auto& image : model.images)
        {
            int width = 0;
            int height = 0;
            int nb_comp = 0;
            u8 *pixels = stbi_load_from_memory(image.data.data(), static_cast<int>(image.data.size()), &width, &height, &nb_comp, 0);


            vulkan::ImageInfo iinfo;
            iinfo.name = "glTF image";
            iinfo.width  = static_cast<u32>(width);
            iinfo.height = static_cast<u32>(height);
            iinfo.depth  = 1;
            iinfo.generate_mip_levels = true;

            if (nb_comp == 1) {
                iinfo.format = vk::Format::eR8Unorm;
            }
            else if (nb_comp == 2) {
                iinfo.format = vk::Format::eR8G8Unorm;
            }
            else if (nb_comp == 3) {
                stbi_image_free(pixels);
                int wanted_nb_comp = 4;
                pixels = stbi_load_from_memory(image.data.data(), static_cast<int>(image.data.size()), &width, &height, &nb_comp, wanted_nb_comp);
                iinfo.format = vk::Format::eR8G8B8A8Unorm;
                nb_comp = wanted_nb_comp;
            }
            else if (nb_comp == 4) {
                iinfo.format = vk::Format::eR8G8B8A8Unorm;
            }

            image.image_h = api.create_image(iinfo);
            api.upload_image(image.image_h, pixels, static_cast<usize>(width * height * nb_comp));
            api.generate_mipmaps(image.image_h);

//       n
//       1           grey
//       2           grey, alpha
//       3           red, green, blue
//       4           red, green, blue, alpha

            stbi_image_free(pixels);
        }
    }

    vulkan::ProgramInfo pinfo{};
    pinfo.vertex_shader = api.create_shader("shaders/gltf.vert.spv");
    pinfo.fragment_shader = api.create_shader("shaders/gltf.frag.spv");

    pinfo.push_constant({/*.stages = */ vk::ShaderStageFlagBits::eFragment, /*.offset = */ 0, /*.size = */ sizeof(MaterialPushConstant)});

    pinfo.binding({/*.set = */ vulkan::SHADER_DESCRIPTOR_SET, /*.slot = */ 0, /*.stages = */ vk::ShaderStageFlagBits::eVertex,/*.type = */ vk::DescriptorType::eUniformBufferDynamic, /*.count = */ 1});
    pinfo.binding({/*.set = */ vulkan::DRAW_DESCRIPTOR_SET, /*.slot = */ 0, /*.stages = */ vk::ShaderStageFlagBits::eVertex,/*.type = */ vk::DescriptorType::eUniformBufferDynamic, /*.count = */ 1});
    pinfo.binding({/* .set = */ vulkan::DRAW_DESCRIPTOR_SET, /*.slot = */ 1, /*.stages = */ vk::ShaderStageFlagBits::eFragment, /*.type = */ vk::DescriptorType::eCombinedImageSampler, /*.count = */ 1});

    pinfo.vertex_stride(sizeof(GltfVertex));
    pinfo.vertex_info({vk::Format::eR32G32B32Sfloat, MEMBER_OFFSET(GltfVertex, position)});
    pinfo.vertex_info({vk::Format::eR32G32B32Sfloat, MEMBER_OFFSET(GltfVertex, normal)});
    pinfo.vertex_info({vk::Format::eR32G32Sfloat, MEMBER_OFFSET(GltfVertex, uv0)});
    pinfo.vertex_info({vk::Format::eR32G32Sfloat, MEMBER_OFFSET(GltfVertex, uv1)});
    pinfo.vertex_info({vk::Format::eR32G32B32A32Sfloat, MEMBER_OFFSET(GltfVertex, joint0)});
    pinfo.vertex_info({vk::Format::eR32G32B32A32Sfloat, MEMBER_OFFSET(GltfVertex, weight0)});
    pinfo.enable_depth = true;

    model.program = api.create_program(std::move(pinfo));
}

void Renderer::destroy_model()
{
    for (auto& image: model.images) {
        api.destroy_image(image.image_h);
    }
    api.destroy_buffer(model.vertex_buffer);
    api.destroy_buffer(model.index_buffer);
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

Model load_model(const char *c_path)
{
    Model model;
    fs::path path{c_path};


    std::ifstream f{path};
    json j;
    f >> j;

    // Load buffers file into memory
    for (const auto &json_buffer : j["buffers"]) {
        Buffer buf;
        buf.byte_length = json_buffer["byteLength"];

        const std::string buffer_name = json_buffer["uri"];
        fs::path buffer_path = path.replace_filename(buffer_name);
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

                u32 count = accessor["count"];
                usize acc_offset = accessor["byteOffset"];
                usize view_offset = view["byteOffset"];
                usize offset = acc_offset + view_offset;

                auto *indices = reinterpret_cast<u16*>(&buffer.data[offset]);
                for (u32 i = 0; i < count; i++) {
                    model.indices.push_back(indices[i]);
                }

                primitive.index_count = count;
            }

            mesh.primitives.push_back(std::move(primitive));
        }
        model.meshes.push_back(std::move(mesh));
    }

    for (const auto &json_node : j["nodes"]) {
	Node node;

	node.mesh = json_node["mesh"];

	if (json_node.count("matrix")) {
	}

	if (json_node.count("translation")) {
	    auto translation_factors = json_node["translation"];
	    node.translation.x = translation_factors[0];
	    node.translation.y = translation_factors[1];
	    node.translation.z = translation_factors[2];
	}

	if (json_node.count("rotation")) {
            const auto& rotation = json_node["rotation"];
            node.rotation = glm::quat(rotation[0], rotation[1], rotation[2], rotation[3]);
	}

	if (json_node.count("scale")) {
	    auto scale_factors = json_node["scale"];
	    node.scale.x = scale_factors[0];
	    node.scale.y = scale_factors[1];
	    node.scale.z = scale_factors[2];
	}

        if (json_node.count("children")) {
            const auto& children =  json_node["children"];
            node.children.reserve(children.size());
            for (u32 child_i : children) {
                node.children.push_back(child_i);
            }
        }

	model.nodes.push_back(std::move(node));
    }

    for (const auto& json_sampler : j["samplers"]) {
        Sampler sampler;
        sampler.mag_filter = json_sampler["magFilter"];
        sampler.min_filter = json_sampler["minFilter"];
        sampler.wrap_s = json_sampler["wrapS"];
        sampler.wrap_t = json_sampler["wrapT"];
        model.samplers.push_back(std::move(sampler));
    }

    // Load images file into memory
    for (const auto& json_image : j["images"]) {
        Image image;

        std::string type = json_image["mimeType"];
        if (type == "image/jpeg") {
        }
        else if (type == "image/png") {
        }
        else {
            throw std::runtime_error("unsupported image type: " + type);
        }

        const std::string image_name = json_image["uri"];
        fs::path image_path = path.replace_filename(image_name);
        image.data = tools::read_file(image_path);

        model.images.push_back(std::move(image));
    }

    for (const auto& json_texture : j["textures"]) {
        Texture texture;
        texture.sampler = json_texture["sampler"];
        texture.image = json_texture["source"];
        model.textures.push_back(std::move(texture));
    }

    for (const auto& json_material : j["materials"]) {
        Material material;

        if (json_material.count("pbrMetallicRoughness")) {
            const auto& json_pbr = json_material["pbrMetallicRoughness"];
            auto base_color_factors = json_pbr["baseColorFactor"];
            material.base_color_factor.r = base_color_factors[0];
            material.base_color_factor.g = base_color_factors[1];
            material.base_color_factor.b = base_color_factors[2];
            material.base_color_factor.a = base_color_factors[3];

            if (json_pbr.count("baseColorTexture")) {
                u32 i_texture = json_pbr["baseColorTexture"]["index"];
                material.base_color_texture = i_texture;
            }
        }

        model.materials.push_back(std::move(material));
    }

    usize scene_i = j["scene"];
    const auto& scene_json = j["scenes"][scene_i];
    for (usize node_i : scene_json["nodes"]) {
	model.scene.push_back(node_i);
    }

    return model;
}
} // namespace my_app
