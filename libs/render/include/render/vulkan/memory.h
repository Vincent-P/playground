#pragma once

using VmaAllocation = struct VmaAllocation_T *;
namespace vulkan
{
enum MemoryUsage
{
	UNKNOWN              = 0,
	GPU_ONLY             = 1,
	CPU_ONLY             = 2,
	CPU_TO_GPU           = 3,
	GPU_TO_CPU           = 4,
	GPU_LAZILY_ALLOCATED = 6,
	MAX_ENUM             = 0x7FFFFFFF
};
using Allocation = VmaAllocation;
} // namespace vulkan
