#include "rhi/shader.h"

#include "rhi/device.h"
#include "rhi/utils.h"
#include "cross/mapped_file.h"
#include "exo/string_view.h"

#include <volk.h>

namespace
{
static Vec<u8> read_file(const exo::StringView &path)
{
	auto mapped_file = cross::MappedFile::open(path);

	if (mapped_file && mapped_file->size) {
		auto res = Vec<u8>::with_length(mapped_file->size);
		std::memcpy(res.data(), mapped_file->base_addr, mapped_file->size);
		return res;
	}

	return {};
}
} // namespace

namespace rhi
{
Handle<Shader> Device::create_shader(exo::StringView path)
{
	auto bytecode = read_file(path);

	VkShaderModuleCreateInfo info = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
	info.codeSize                 = bytecode.len();
	info.pCode                    = reinterpret_cast<const u32 *>(bytecode.data());

	VkShaderModule vkhandle = VK_NULL_HANDLE;
	vk_check(vkCreateShaderModule(device, &info, nullptr, &vkhandle));

	return shaders.add({
		.filename = exo::String(path),
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
}; // namespace rhi
