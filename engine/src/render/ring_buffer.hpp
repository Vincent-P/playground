#pragma once

#include "base/types.hpp"
#include "base/handle.hpp"

#include "render/vulkan/resources.hpp"
#include "render/vulkan/commands.hpp"

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
    u32 offset = 0;
    u32 usage = 0;
    u32 last_frame_end = 0;
    u32 last_frame_size = 0;
    u32 this_frame_size = 0;
    Handle<gfx::Buffer> buffer;
    bool should_align = true;

    static RingBuffer create(gfx::Device &device, const RingBufferDescription &desc, bool align = true);
    std::pair<void*, u32> allocate(gfx::Device &device, usize len);
    void start_frame();
    void end_frame();

    template<typename T>
    std::pair<T*, u32> allocate(gfx::Device &device)
    {
        auto [void_ptr, alloc_offset] = allocate(device, sizeof(T));
        return std::make_pair(reinterpret_cast<T*>(void_ptr), alloc_offset);
    }
};
