#include "render/gpu_pool.hpp"

#include "base/logger.hpp"

#include "render/vulkan/device.hpp"
#include "render/vulkan/resources.hpp"
#include "vulkan/vulkan_core.h"

#include <fmt/format.h>
#include <memory>

GpuPool GpuPool::create(gfx::Device &device, const GpuPoolDescription &desc)
{
    GpuPool pool;

    pool.name         = desc.name;
    pool.size         = desc.size;
    pool.element_size = desc.element_size;
    pool.capacity     = desc.size / desc.element_size;

    pool.host = device.create_buffer({
        .name         = fmt::format("{} host", pool.name),
        .size         = desc.size,
        .usage        = gfx::source_buffer_usage,
        .memory_usage = VMA_MEMORY_USAGE_CPU_ONLY,
    });

    pool.device = device.create_buffer({
        .name         = fmt::format("{} device", pool.name),
        .size         = desc.size,
        .usage        = desc.gpu_usage,
        .memory_usage = VMA_MEMORY_USAGE_GPU_ONLY,
    });

    pool.data = device.map_buffer<u8>(pool.host);

    auto &free_list_head = *reinterpret_cast<GpuPool::FreeList*>(pool.data);
    free_list_head.size = pool.capacity;
    free_list_head.next = u32_invalid;

    return pool;
}

std::pair<bool, u32> GpuPool::allocate(u32 element_count)
{
    if (this->length + element_count > this->capacity)
    {
        logger::error("[GpuPool] allocate(): Pool ({}) is full.\n", this->name);
        return {false, u32_invalid};
    }

    u32 offset = free_list_head_offset;
    auto *free_list_head = reinterpret_cast<GpuPool::FreeList*>(this->data + free_list_head_offset * this->element_size);

    // Search for free block
    while (free_list_head->size < element_count && free_list_head->next != u32_invalid)
    {
        free_list_head = reinterpret_cast<GpuPool::FreeList*>(this->data + free_list_head->next * this->element_size);
        offset = free_list_head->next;
    }

    if (free_list_head->size < element_count)
    {
        logger::error("[GpuPool] allocate(): Pool ({}) is full.\n", this->name);
        return {false, u32_invalid};
    }

    // Fit the allocation to the needed size
    if (free_list_head->size > element_count)
    {
        auto *new_head = reinterpret_cast<GpuPool::FreeList*>(this->data + (offset + element_count) * this->element_size);
        new_head->size = free_list_head->size - element_count;
        new_head->next = free_list_head_offset;
        free_list_head_offset = offset + element_count;
    }

    if (this->valid_allocations.contains(offset) == true)
    {
        logger::error("[GpuPool] allocate(): overwriting allocation at offset {}\n", offset);
    }

    this->valid_allocations[offset] = element_count;
    this->length += element_count;

    return {true, offset};
}

void GpuPool::free(u32 offset)
{
    if (this->valid_allocations.contains(offset) == false)
    {
        logger::error("[GpuPool] free(): invalid offset ({})\n", offset);
        return;
    }

    u32 element_count = this->valid_allocations[offset];

    auto *new_head = reinterpret_cast<GpuPool::FreeList*>(this->data + offset * this->element_size);
    new_head->size = element_count;
    new_head->next = this->free_list_head_offset;

    this->free_list_head_offset = offset;

    this->valid_allocations.erase(offset);
    this->dirty_allocations.erase(offset);
    this->length -= element_count;
}

bool GpuPool::update(u32 offset, u32 element_count, const void* data)
{
    if (this->valid_allocations.contains(offset) == false)
    {
        logger::error("[GpuPool] update(): invalid offset ({})\n", offset);
        return false;
    }

    u32 alloc_element_count = this->valid_allocations[offset];
    if (element_count > alloc_element_count)
    {
        logger::error("[GpuPool] update(): trying to update {} elements but allocation's size is {}\n", element_count, alloc_element_count);
        return false;
    }

    void *dst = this->data + offset * this->element_size;
    std::memcpy(dst, data, element_count * this->element_size);

    this->dirty_allocations.insert(offset);

    return true;
}

bool GpuPool::is_up_to_date(u32 offset)
{
    return this->dirty_allocations.contains(offset);
}

void GpuPool::upload_changes(gfx::TransferWork &cmd)
{
    if (this->has_changes() == false)
    {
        return;
    }

    Vec<std::pair<u32, u32>> copies;
    copies.reserve(dirty_allocations.size());

    usize i_alloc = 0;
    for (const u32 dirty_alloc : dirty_allocations)
    {
        if (this->valid_allocations.contains(dirty_alloc) == false)
        {
            logger::error("[GpuPool] upload_changes(): invalid offset ({}) in dirty allocations.\n", dirty_alloc);
        }
        copies.emplace_back(dirty_alloc * this->element_size, this->valid_allocations[dirty_alloc] * this->element_size);
        i_alloc += 1;
    }

    dirty_allocations.clear();

    cmd.barrier(this->device, gfx::BufferUsage::TransferDst);
    cmd.copy_buffer(this->host, this->device, copies);
}
