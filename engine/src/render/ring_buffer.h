#pragma once

#include <exo/maths/numerics.h>
#include <exo/handle.h>

#include <string_view>

namespace vulkan {struct Buffer; struct Device;};
namespace gfx = vulkan;

struct RingBufferDescription
{
    std::string_view name;
    usize size;
    u32 gpu_usage;
};

struct RingBuffer
{
    std::string name;
    usize size = 0;
    usize offset = 0;
    u32 usage = 0;
    usize last_frame_end = 0;
    usize last_frame_size = 0;
    usize this_frame_size = 0;
    Handle<gfx::Buffer> buffer;
    bool should_align = true;

    static RingBuffer create(gfx::Device &device, const RingBufferDescription &desc, bool align = true);
    std::pair<void*, usize> allocate(gfx::Device &device, usize len);
    void start_frame();
    void end_frame();
};
