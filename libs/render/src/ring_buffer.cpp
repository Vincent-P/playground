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
		.memory_usage = vulkan::MemoryUsage::PREFER_HOST,
	});

	buf.frame_size_allocated.resize(desc.frame_queue_length);
	for (auto &v : buf.frame_size_allocated) {
		v = 0;
	}

	buf.start    = reinterpret_cast<u8 *>(device.map_buffer(buf.buffer));
	buf.cursor   = 0;
	buf.capacity = desc.size;
	return buf;
}

static u64 ringbuffer_size_from(RingBuffer &ringbuffer, u64 from)
{
	if (ringbuffer.full) {
		return ringbuffer.capacity;
	} else if (from >= ringbuffer.tail) {
		return from - ringbuffer.tail;
	} else {
		return ringbuffer.capacity - (ringbuffer.tail - from);
	}
}

std::pair<exo::Span<u8>, usize> RingBuffer::allocate(usize len, usize alignment)
{
	u64 offset    = cursor;
	u64 allocated = 0;

	// Align the offset to the desired alignment
	if ((offset % alignment) != 0) {
		const u64 diff = alignment - (offset % alignment);
		offset += diff;
		allocated += diff;
	}

	const auto free_space = this->capacity - ringbuffer_size_from(*this, offset);

	if (len >= free_space) {
		return std::make_pair(exo::Span<u8>(), 0);
	}

	if (offset + len < this->capacity) {
		// free space at the end
	} else if (this->tail >= len) {
		// the len is not enough at the end
		allocated += (this->capacity - offset);
		offset = 0;
	} else {
		// There is not enough space???
		ASSERT(false);
	}

	allocated += len;

	const usize frame_count = this->frame_size_allocated.size();

	this->frame_size_allocated[this->i_frame % frame_count] += allocated;

	this->cursor = offset + len;

	// Do these happen in practice?
	// ASSERT(this->frame_size_allocated[this->i_frame % frame_count] <= this->capacity);
	if (this->cursor >= this->capacity) {
		ASSERT(this->cursor == this->capacity);
		this->cursor = 0;
	}
	if (this->cursor == this->tail) {
		this->full = true;
	}

	ASSERT(offset % alignment == 0);
	return std::make_pair(exo::Span<u8>(this->start + offset, len), offset);
}

void RingBuffer::start_frame()
{
	this->i_frame += 1;

	const usize frame_count            = this->frame_size_allocated.size();
	const u64   allocated_latest_frame = this->frame_size_allocated[this->i_frame % frame_count];
	if (allocated_latest_frame > 0) {
		this->tail = (this->tail + allocated_latest_frame) % this->capacity;
		this->full = false;
	}
	this->frame_size_allocated[this->i_frame % frame_count] = 0;
}
