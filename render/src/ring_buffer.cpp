#include "render/ring_buffer.h"

#include <exo/maths/pointer.h>

#include "render/vulkan/buffer.h"
#include "render/vulkan/device.h"

RingBuffer RingBuffer::create(gfx::Device &device, const RingBufferDescription &desc)
{
	RingBuffer buf;
	buf.name   = desc.name;
	buf.usage  = desc.gpu_usage;
	buf.buffer = device.create_buffer({
		.name         = buf.name,
		.size         = desc.size,
		.usage        = buf.usage,
		.memory_usage = vulkan::MemoryUsage::CPU_TO_GPU,
	});

	buf.frame_start.resize(desc.frame_queue_length + 1);
	buf.buffer_start = reinterpret_cast<u8 *>(device.map_buffer(buf.buffer));
	buf.buffer_end   = buf.buffer_start + desc.size;
	buf.cursor       = buf.buffer_start;
	buf.size         = desc.size;
	return buf;
}

std::pair<void *, usize> RingBuffer::allocate(usize len, usize alignment)
{
	u64 dist = static_cast<u64>(cursor - buffer_start) % alignment;
	auto new_cursor = this->cursor;

	if (dist != 0) {
		new_cursor += alignment - dist;
	}

	// wrap cursor at the end the buffer
	if (new_cursor + len > buffer_end) {
		new_cursor = buffer_start;
	}

	// check if there is enough space
	const auto frame_size           = frame_start.size();
	const u8  *previous_frame_start = frame_start[(i_frame + frame_size - 1) % frame_size];
	if (previous_frame_start && new_cursor < previous_frame_start && new_cursor + len > previous_frame_start) {
		ASSERT(!"Not enough space");
		return std::make_pair(nullptr, 0);
	}

	auto *offset = new_cursor;
	new_cursor += len;

	cursor = new_cursor;

	ASSERT((offset - buffer_start) % static_cast<i64>(alignment) == 0);
	return std::make_pair(offset, offset - buffer_start);
}

void RingBuffer::start_frame()
{
	i_frame += 1;
	frame_start[i_frame % frame_start.size()] = cursor;
}
