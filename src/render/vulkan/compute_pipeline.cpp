#include "render/vulkan/descriptor_set.hpp"
#include "render/vulkan/resources.hpp"
#include "render/vulkan/device.hpp"
#include "render/vulkan/utils.hpp"
#include "vulkan/vulkan_core.h"

namespace vulkan
{
Handle<ComputeProgram> Device::create_program(const ComputeState &compute_state)
{
    const auto &shader = *shaders.get(compute_state.shader);

    DescriptorSet set = create_descriptor_set(*this, compute_state.descriptors);

    std::array sets = {global_set.vklayout, set.layout};

    VkPipelineLayoutCreateInfo pipeline_layout_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipeline_layout_info.setLayoutCount = sets.size();
    pipeline_layout_info.pSetLayouts    = sets.data();

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout));

    VkComputePipelineCreateInfo pipeline_info = {.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipeline_info.stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline_info.stage.module = shader.vkhandle;
    pipeline_info.stage.pName = "main";
    pipeline_info.layout = pipeline_layout;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline));

    return compute_programs.add({
        .pipeline = pipeline,
        .pipeline_layout = pipeline_layout,
        .descriptor_set = std::move(set),
    });
}

void Device::destroy_program(Handle<ComputeProgram> program_handle)
{
    if (auto *program = compute_programs.get(program_handle))
    {
        vkDestroyPipeline(device, program->pipeline, nullptr);
        vkDestroyPipelineLayout(device, program->pipeline_layout, nullptr);
        destroy_descriptor_set(*this, program->descriptor_set);

        compute_programs.remove(program_handle);
    }
}
}
