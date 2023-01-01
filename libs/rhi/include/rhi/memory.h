#pragma once
using VmaAllocation = struct VmaAllocation_T *;
namespace rhi
{
enum MemoryUsage
{
	AUTO          = 7,
	PREFER_DEVICE = 8,
	PREFER_HOST   = 9,
	MAX_ENUM      = 0x7FFFFFFF
};
using Allocation = VmaAllocation;
} // namespace rhi
