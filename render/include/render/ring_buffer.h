#pragma once

#include <exo/collections/dynamic_array.h>
#include <exo/collections/handle.h>
#include <exo/maths/numerics.h>

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
	std::string         name;
	u32                 usage = 0;
	Handle<gfx::Buffer> buffer;

	u8 *cursor;
	u8 *buffer_start;
	u8 *buffer_end;

	u32                        i_frame = 0;
	exo::DynamicArray<u8 *, 3> frame_start; // keep track of the start of each frame

	static RingBuffer create(gfx::Device &device, const RingBufferDescription &desc);

	// The alignment field is necessary to put different data structures inside the same buffer, and be able to index
	// them from the returned offset The default value is 256 so that the returned offset can be used as a dynamic
	// uniform buffer
	std::pair<void *, usize> allocate(gfx::Device &device, usize len, usize subalignment = 256);
	void                     start_frame();
};
