#include "render/vulkan/shader.h"

#include "render/vulkan/device.h"
#include "render/vulkan/utils.h"

#include <filesystem>
#include <fstream>
#include <volk.h>

namespace
{
static Vec<u8> read_file(const std::filesystem::path &path)
{
	std::ifstream file{path, std::ios::binary};
	if (file.fail()) {
		return {};
	}

	std::streampos begin;
	std::streampos end;
	begin = file.tellg();
	file.seekg(0, std::ios::end);
	end = file.tellg();

	auto result = Vec<u8>::with_length(static_cast<usize>(end - begin));
	if (result.is_empty()) {
		return {};
	}

	file.seekg(0, std::ios::beg);
	file.read(reinterpret_cast<char *>(result.data()), end - begin);
	file.close();

	return result;
}
} // namespace

namespace vulkan
{
Handle<Shader> Device::create_shader(std::string_view path)
{
	auto bytecode = read_file(path);

	VkShaderModuleCreateInfo info = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
	info.codeSize                 = bytecode.len();
	info.pCode                    = reinterpret_cast<const u32 *>(bytecode.data());

	VkShaderModule vkhandle = VK_NULL_HANDLE;
	vk_check(vkCreateShaderModule(device, &info, nullptr, &vkhandle));

	return shaders.add({
		.filename = std::string(path),
		.vkhandle = vkhandle,
		.bytecode = std::move(bytecode),
	});
}

void Device::reload_shader(Handle<Shader> shader_handle)
{
	auto &shader = shaders.get(shader_handle);
	vkDestroyShaderModule(device, shader.vkhandle, nullptr);

	shader.bytecode = read_file(shader.filename);

	VkShaderModuleCreateInfo info = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
	info.codeSize                 = shader.bytecode.len();
	info.pCode                    = reinterpret_cast<const u32 *>(shader.bytecode.data());

	vk_check(vkCreateShaderModule(device, &info, nullptr, &shader.vkhandle));
}

void Device::destroy_shader(Handle<Shader> shader_handle)
{
	auto &shader = shaders.get(shader_handle);
	vkDestroyShaderModule(device, shader.vkhandle, nullptr);
	shaders.remove(shader_handle);
}
}; // namespace vulkan
