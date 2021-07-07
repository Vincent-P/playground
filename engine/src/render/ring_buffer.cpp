#include "render/ring_buffer.hpp"

#include "render/vulkan/device.hpp"

RingBuffer RingBuffer::create(gfx::Device &device, const RingBufferDescription &desc)
{
    RingBuffer buf;
    buf.name = desc.name;
    buf.size = desc.size;
    buf.usage = desc.gpu_usage;
    buf.buffer = device.create_buffer({
            .name         = buf.name,
            .size         = desc.size,
            .usage        = buf.usage,
            .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
        });
    return buf;
}

std::pair<void*, usize> RingBuffer::allocate(gfx::Device &device, usize len)
{
    usize aligned_len = ((len / 256u) + 1u) * 256u;

    // TODO: handle the correct number of frame instead of ALWAYS the last one
    // check that we dont overwrite previous frame's content
    usize last_frame_start = last_frame_end - last_frame_size;
    assert(offset + aligned_len < last_frame_start + size);

    // if offset + aligned_len is outside the buffer go back to the beginning (ring buffer)
    if ((offset % size) + aligned_len >= size)
    {
        offset = ((offset / size) + 1u) * size;
    }

    usize allocation_offset = offset % size;

    void *dst = device.map_buffer<u8>(buffer) + allocation_offset;

    offset += aligned_len;
    this_frame_size += aligned_len;

    return std::make_pair(dst, allocation_offset);
}

void RingBuffer::start_frame()
{
    // reset dynamic buffer frame size
    this_frame_size = 0;
}

void RingBuffer::end_frame()
{
    last_frame_end  = offset;
    last_frame_size = this_frame_size;
}
