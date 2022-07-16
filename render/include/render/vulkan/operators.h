#pragma once

#include <volk.h>

namespace vulkan
{
bool operator==(const VkPipelineShaderStageCreateInfo &a, const VkPipelineShaderStageCreateInfo &b);
bool operator==(const VkDescriptorBufferInfo &a, const VkDescriptorBufferInfo &b);
bool operator==(const VkDescriptorImageInfo &a, const VkDescriptorImageInfo &b);
bool operator==(const VkExtent3D &a, const VkExtent3D &b);
bool operator==(const VkImageSubresourceRange &a, const VkImageSubresourceRange &b);
bool operator==(const VkImageCreateInfo &a, const VkImageCreateInfo &b);
bool operator==(const VkComputePipelineCreateInfo &a, const VkComputePipelineCreateInfo &b);
bool operator==(const VkFramebufferCreateInfo &a, const VkFramebufferCreateInfo &b);
bool operator==(const VkClearValue &a, const VkClearValue &b);
} // namespace vulkan
