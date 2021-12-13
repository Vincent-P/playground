#pragma once

#include <exo/collections/handle.h>
#include <exo/memory/buddy_allocator.h>

namespace vulkan {struct Buffer; struct Device;}
namespace gfx = vulkan;

namespace vulkan {struct Device;}

struct UnifiedBufferStorage
{
    exo::BuddyAllocator allocator;
    Handle<gfx::Buffer> buffer;
    u32 element_size = 0;

    static UnifiedBufferStorage create(gfx::Device &device, std::string name, usize size_in_bytes, u32 element_size, u32 gpu_usage = 0);
    u32 allocate(usize nb_element);
    void free(u32 offset);
    void destroy();
};
