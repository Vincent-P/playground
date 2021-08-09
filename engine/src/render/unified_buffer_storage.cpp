#include "render/unified_buffer_storage.h"

#include "render/vulkan/device.h"

#include <exo/buddy_allocator.h>


UnifiedBufferStorage UnifiedBufferStorage::create(gfx::Device &device, std::string name, u32 size_in_bytes, u32 element_size, u32 gpu_usage)
{
    UnifiedBufferStorage storage = {};
    storage.allocator = BuddyAllocator::create(size_in_bytes);
    storage.buffer = device.create_buffer({
            .name = name,
            .size = size_in_bytes,
            .usage = gfx::storage_buffer_usage | gpu_usage,
        });
    storage.element_size = element_size;
    return storage;
}

u32 UnifiedBufferStorage::allocate(u32 nb_element)
{
    u32 byte_offset = allocator.allocate(nb_element * this->element_size);
    assert(byte_offset % this->element_size == 0);
    return  byte_offset / this->element_size;
}

void UnifiedBufferStorage::free(u32 offset)
{
    allocator.free(offset);
}
