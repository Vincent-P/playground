#pragma once
#include <volk.h>

namespace vulkan
{

// DX12-like fence used for CPU/CPU, CPU/GPU, or GPU/GPU synchronization
struct Fence
{
	VkSemaphore timeline_semaphore = VK_NULL_HANDLE;
	u64         value              = 0;
};

} // namespace vulkan
