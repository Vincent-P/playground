#pragma once
#include "vulkan/vulkan_core.h"

namespace vulkan
{
struct QueryPool
{
	VkQueryPool vkhandle = VK_NULL_HANDLE;
	u32         capacity = 0;
};
} // namespace vulkan
