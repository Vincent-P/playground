#pragma once

#include <exo/collections/dynamic_array.h>
#include <exo/collections/handle.h>
#include <exo/maths/numerics.h>

#include <span>
#include <string_view>

namespace vulkan
{
struct Buffer;
struct Device;
}; // namespace vulkan
namespace gfx = vulkan;

struct RingBufferDescription
{
	std::string_view name;
	usize            size;
	u32              gpu_usage;
	u32              frame_queue_length;
};

struct RingBuffer
{
	std::string         name     = "unnamed";
	u32                 usage    = 0;
	Handle<gfx::Buffer> buffer   = {};
	usize               capacity = 0;

	u8  *start  = nullptr;
	u64  cursor = 0;
	u64  tail   = 0;
	bool full   = false;

	u32                       i_frame = 0;
	exo::DynamicArray<u64, 3> frame_size_allocated;

	static RingBuffer create(gfx::Device &device, const RingBufferDescription &desc);

	// The alignment field is necessary to put different data structures inside the same buffer, and be able to index
	// them from the returned offset The default value is 256 so that the returned offset can be used as a dynamic
	// uniform buffer
	std::pair<std::span<u8>, usize> allocate(usize len, usize subalignment = 256);
	void                            start_frame();
};
