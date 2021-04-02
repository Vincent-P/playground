#pragma once

#include "base/types.hpp"
#include "base/handle.hpp"

#include "render/vulkan/resources.hpp"
#include "render/vulkan/commands.hpp"

namespace vulkan {struct Buffer; struct Device;}

namespace gfx = vulkan;

struct StreamingBuffer
{
    u32 size;
    u32 element_size;
    uint current;
    uint capacity;
    u32  transfer_start = u32_invalid;
    u32  transfer_end = u32_invalid;
    u64  transfer_done = u32_invalid;
    Handle<gfx::Buffer> buffer;
    Handle<gfx::Buffer> buffer_staging;
};

StreamingBuffer streaming_buffer_create(gfx::Device &device, std::string_view name, u32 size, u32 element_size, u32 usage = gfx::storage_buffer_usage);
bool streaming_buffer_allocate(gfx::Device &device, StreamingBuffer &streaming_buffer, u32 nb_elements, u32 element_size, const void *src);
void streaming_buffer_upload(gfx::TransferWork &cmd, StreamingBuffer &streaming_buffer);
inline bool streaming_buffer_has_transfer(const StreamingBuffer &b) { return b.transfer_start != u32_invalid; }
