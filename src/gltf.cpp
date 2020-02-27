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

void Renderer::draw_model(Model &model)
{
    // last program
    // bind program

    for (const auto& mesh : model.meshes)
    {
	for (const auto& primitive : mesh.primitives)
	{
	    // if program != last program then bind program

	    const auto& index_acc = model.accessors[primitive.indices_accessor];
	    const auto& index_view = model.buffer_views[index_acc.buffer_view];
	    vulkan::BufferH index_h = model.buffers[index_view.buffer].buffer_h;
	    u32 index_offset = index_acc.byte_offset + index_view.byte_offset;
	    api.bind_index_buffer(index_h, index_offset);

	    const auto& vertex_acc = model.accessors[primitive.indices_accessor];
	    const auto& vertex_view = model.buffer_views[vertex_acc.buffer_view];
	    vulkan::BufferH vertex_h = model.buffers[vertex_view.buffer].buffer_h;
	    u32 vertex_offset = vertex_acc.byte_offset + vertex_view.byte_offset;
	    api.bind_vertex_buffer(vertex_h, vertex_offset);

	    u32 index_count = index_acc.count;
	    api.draw_indexed(index_count, 1, 0, 0, 0);

	}
    }
}

void Renderer::load_model(Model &model)
{
    for (auto& buffer : model.buffers)
    {
	vulkan::BufferInfo info;
	info.name         = "Staging Buffer";
	info.size         = buffer.byte_length;
	info.usage        = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;
	info.memory_usage = VMA_MEMORY_USAGE_GPU_ONLY;
	buffer.buffer_h   = api.create_buffer(info);
	api.upload_buffer(buffer.buffer_h, buffer.data.data(), buffer.data.size());
    }

    vulkan::ProgramInfo pinfo{};
    pinfo.vertex_shader   = api.create_shader("shaders/sponza.vert.spv");
    pinfo.fragment_shader = api.create_shader("shaders/sponza.frag.spv");
    pinfo.push_constant({/*.stages = */ vk::ShaderStageFlagBits::eVertex, /*.offset = */ 0, /*.size = */ 4 * sizeof(float)});
    pinfo.binding({/*.slot = */ 0, /*.stages = */ vk::ShaderStageFlagBits::eFragment,/*.type = */ vk::DescriptorType::eCombinedImageSampler, /*.count = */ 1});

    pinfo.vertex_stride(sizeof(ImDrawVert));
    pinfo.vertex_info({vk::Format::eR32G32Sfloat, MEMBER_OFFSET(ImDrawVert, pos)});
    pinfo.vertex_info({vk::Format::eR32G32Sfloat, MEMBER_OFFSET(ImDrawVert, uv)});
    pinfo.vertex_info({vk::Format::eR8G8B8A8Unorm, MEMBER_OFFSET(ImDrawVert, col)});

    vulkan::ProgramH program = api.create_program(std::move(pinfo));

}

std::optional<AccessorType> accessor_type_from_str(const std::string& string);

Model load_model(const char *path)
{
    fs::path gltf_path{path};

    Model model;

    std::ifstream f{path};
    json j;
    f >> j;

    for (const auto &json_buffer : j["buffers"]) {
	Buffer buf;
	buf.byte_length = json_buffer["byteLength"];

	fs::path buffer_file = gltf_path.replace_filename(json_buffer["uri"]);
	buf.data             = tools::read_file(path);

	model.buffers.push_back(std::move(buf));
    }

    for (const auto &json_buffer_view : j["bufferViews"]) {
	BufferView buf_view;
	buf_view.buffer      = json_buffer_view["buffer"];
	buf_view.byte_length = json_buffer_view["byteLength"];
	buf_view.byte_offset = json_buffer_view["byteOffset"];
	model.buffer_views.push_back(std::move(buf_view));
    }

    for (const auto &json_accessor : j["accessors"]) {
	Accessor accessor;
	accessor.buffer_view    = json_accessor["bufferView"];
	accessor.component_type = json_accessor["componentType"];
	accessor.type           = json_accessor["type"];
	accessor.byte_offset    = json_accessor["byteOffset"];
	accessor.count          = json_accessor["count"];
        model.accessors.push_back(std::move(accessor));
    }

    for (const auto &json_mesh : j["meshes"]) {
        Mesh mesh;
	for (const auto &json_primitive : json_mesh["primitives"]) {
	    Primitive primitive;
	    primitive.indices_accessor = json_primitive["indices"];
	    primitive.material         = json_primitive["material"];
	    primitive.mode             = json_primitive["mode"];

	    for (const auto &json_attribute : json_primitive["attributes"]) {
		primitive.position_accessor = json_attribute["POSITION"];
		primitive.normal_accessor   = json_attribute["NORMAL"];
            }

            mesh.primitives.push_back(std::move(primitive));
        }
        model.meshes.push_back(std::move(mesh));
    }

    return model;
}
} // namespace my_app
