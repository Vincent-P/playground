#include "render/vulkan/descriptor_set.h"
#include "render/vulkan/resources.h"
#include "render/vulkan/device.h"
#include "render/vulkan/utils.h"
#include "vulkan/vulkan_core.h"

namespace vulkan
{
void Device::recreate_program_internal(ComputeProgram &program)
{
    vkDestroyPipeline(device, program.pipeline, nullptr);
    vkDestroyPipelineLayout(device, program.pipeline_layout, nullptr);
    destroy_descriptor_set(*this, program.descriptor_set);

    const auto &shader = *shaders.get(program.state.shader);

    DescriptorSet set = create_descriptor_set(*this, program.state.descriptors);

    std::array sets = {
        global_sets.uniform.layout,
        global_sets.sampled_images.layout,
        global_sets.storage_images.layout,
        global_sets.storage_buffers.layout,
        set.layout,
    };

    VkPushConstantRange push_constant_range;
    push_constant_range.stageFlags = VK_SHADER_STAGE_ALL;
    push_constant_range.offset     = 0;
    push_constant_range.size       = static_cast<u32>(push_constant_layout.size);

    VkPipelineLayoutCreateInfo pipeline_layout_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipeline_layout_info.setLayoutCount = static_cast<u32>(sets.size());
    pipeline_layout_info.pSetLayouts    = sets.data();
    pipeline_layout_info.pushConstantRangeCount     = push_constant_range.size ? 1 : 0;
    pipeline_layout_info.pPushConstantRanges        = &push_constant_range;

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

    program.pipeline = pipeline;
    program.pipeline_layout = pipeline_layout;
    program.descriptor_set = std::move(set);
}

Handle<ComputeProgram> Device::create_program(std::string name, const ComputeState &compute_state)
{
    const auto &shader = *shaders.get(compute_state.shader);

    DescriptorSet set = create_descriptor_set(*this, compute_state.descriptors);

    std::array sets = {
        global_sets.uniform.layout,
        global_sets.sampled_images.layout,
        global_sets.storage_images.layout,
        global_sets.storage_buffers.layout,
        set.layout,
    };

    VkPushConstantRange push_constant_range;
    push_constant_range.stageFlags = VK_SHADER_STAGE_ALL;
    push_constant_range.offset     = 0;
    push_constant_range.size       = static_cast<u32>(push_constant_layout.size);

    VkPipelineLayoutCreateInfo pipeline_layout_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipeline_layout_info.setLayoutCount = static_cast<u32>(sets.size());
    pipeline_layout_info.pSetLayouts    = sets.data();
    pipeline_layout_info.pushConstantRangeCount     = push_constant_range.size ? 1 : 0;
    pipeline_layout_info.pPushConstantRanges        = &push_constant_range;

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

    if (this->vkSetDebugUtilsObjectNameEXT)
    {
        VkDebugUtilsObjectNameInfoEXT ni = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        ni.objectHandle                  = reinterpret_cast<u64>(pipeline);
        ni.objectType                    = VK_OBJECT_TYPE_PIPELINE;
        ni.pObjectName                   = name.c_str();
        VK_CHECK(this->vkSetDebugUtilsObjectNameEXT(device, &ni));
    }

    return compute_programs.add({
        .name = name,
        .state = compute_state,
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
