#include "render/vulkan/pipelines.h"

#include "render/vulkan/descriptor_set.h"
#include "render/vulkan/device.h"
#include "render/vulkan/utils.h"

namespace vulkan
{
void Device::recreate_program_internal(ComputeProgram &program)
{
	vkDestroyPipeline(device, program.pipeline, nullptr);

	const auto &shader = shaders.get(program.state.shader);

	VkComputePipelineCreateInfo pipeline_info = {.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
	pipeline_info.stage                       = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
	pipeline_info.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
	pipeline_info.stage.module                = shader.vkhandle;
	pipeline_info.stage.pName                 = "main";
	pipeline_info.layout                      = global_sets.pipeline_layout;

	VkPipeline pipeline = VK_NULL_HANDLE;
	vk_check(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline));

	program.pipeline = pipeline;
}

Handle<ComputeProgram> Device::create_program(exo::StringView name, const ComputeState &compute_state)
{
	const auto &shader = shaders.get(compute_state.shader);

	VkComputePipelineCreateInfo pipeline_info = {.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
	pipeline_info.stage                       = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
	pipeline_info.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
	pipeline_info.stage.module                = shader.vkhandle;
	pipeline_info.stage.pName                 = "main";
	pipeline_info.layout                      = global_sets.pipeline_layout;

	VkPipeline pipeline = VK_NULL_HANDLE;
	vk_check(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline));

	auto name_string = exo::String{name};

	if (vkSetDebugUtilsObjectNameEXT) {
		VkDebugUtilsObjectNameInfoEXT ni = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
		ni.objectHandle                  = reinterpret_cast<u64>(pipeline);
		ni.objectType                    = VK_OBJECT_TYPE_PIPELINE;
		ni.pObjectName                   = name_string.c_str();
		vk_check(vkSetDebugUtilsObjectNameEXT(device, &ni));
	}

	return compute_programs.add({
		.name     = std::move(name_string),
		.state    = compute_state,
		.pipeline = pipeline,
	});
}

void Device::destroy_program(Handle<ComputeProgram> program_handle)
{
	auto &program = compute_programs.get(program_handle);
	vkDestroyPipeline(device, program.pipeline, nullptr);
	compute_programs.remove(program_handle);
}
} // namespace vulkan
