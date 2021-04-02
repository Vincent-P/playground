#include "render/streaming_buffer.hpp"

#include "render/vulkan/device.hpp"
#include "vulkan/vulkan_core.h"
#include <fmt/format.h>

StreamingBuffer streaming_buffer_create(gfx::Device &device, std::string_view name, u32 size, u32 element_size, u32 usage)
{
    StreamingBuffer buffer;

    buffer.size = size;
    buffer.element_size = element_size;
    buffer.capacity = buffer.size / buffer.element_size;

    buffer.current = 0;

    buffer.buffer = device.create_buffer({
        .name  = fmt::format("{} device", name),
        .size  = size,
        .usage = usage,
    });

    buffer.buffer_staging = device.create_buffer({
        .name  = fmt::format("{} host staging", name),
        .size  = size,
        .usage = gfx::source_buffer_usage,
        .memory_usage = VMA_MEMORY_USAGE_CPU_ONLY,
    });

    return buffer;
}

bool streaming_buffer_allocate(gfx::Device &device, StreamingBuffer &streaming_buffer, u32 nb_elements, u32 element_size, const void *src)
{
    if (streaming_buffer.current + nb_elements > streaming_buffer.capacity)
    {
        return false;
    }

    auto *staging   = device.map_buffer<u8>(streaming_buffer.buffer_staging);

    std::memcpy(staging + streaming_buffer.current * element_size, src, nb_elements * element_size);

    if (streaming_buffer.transfer_start == u32_invalid)
    {
        streaming_buffer.transfer_start = streaming_buffer.current;
    }

    if (streaming_buffer.transfer_end == u32_invalid)
    {
        streaming_buffer.transfer_end = streaming_buffer.current + nb_elements;
    }
    else
    {
        streaming_buffer.transfer_end += nb_elements;
    }

    streaming_buffer.current += nb_elements;
    return true;
}

void streaming_buffer_upload(gfx::TransferWork &cmd, StreamingBuffer &streaming_buffer)
{
    cmd.barrier(streaming_buffer.buffer, gfx::BufferUsage::TransferDst);
    cmd.copy_buffer(streaming_buffer.buffer_staging, streaming_buffer.buffer);
    streaming_buffer.transfer_start = u32_invalid;
    streaming_buffer.transfer_end = u32_invalid;
}
