#pragma once

#include "base/types.hpp"
#include "base/handle.hpp"

#include "render/vulkan/resources.hpp"
#include "render/vulkan/commands.hpp"
#include "render/gfx.hpp"

struct RingBufferDescription
{
    std::string_view name;
    u32 size;
    u32 gpu_usage;
};

struct RingBuffer
{
    std::string name;
    usize size = 0;
    u32 usage = 0;
    u32 last_frame_start = 0;
    u32 last_frame_end = 0;
    u32 last_frame_size = 0;
    u32 this_frame_size = 0;
    u32 offset = 0;
    Handle<gfx::Buffer> buffer;

    static RingBuffer create(gfx::Device &device, const RingBufferDescription &desc);
    std::pair<void*, usize> allocate(gfx::Device &device, usize len);
    void start_frame();
    void end_frame();

    template<typename T>
    std::pair<T*, usize> allocate(gfx::Device &device)
    {
        auto [void_ptr, offset] = allocate(device, sizeof(T));
        return std::make_pair(reinterpret_cast<T*>(void_ptr), offset);
    }
};
