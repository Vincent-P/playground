#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#pragma clang diagnostic pop

namespace my_app
{
    class Buffer
    {
        public:
        Buffer()
            : allocator(nullptr)
            , mapped(nullptr)
            , size(0)
            , buf_usage()
            , mem_usage()
        {
        }

        Buffer(VmaAllocator& _allocator, size_t _size, vk::BufferUsageFlags _buf_usage, VmaMemoryUsage _mem_usage = VMA_MEMORY_USAGE_CPU_TO_GPU)
            : allocator(&_allocator)
            , mapped(nullptr)
            , size(_size)
            , buf_usage(_buf_usage)
            , mem_usage(_mem_usage)
        {
            vk::BufferCreateInfo ci{};
            ci.usage = buf_usage;
            ci.size = size;

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = mem_usage;

            vmaCreateBuffer(*allocator,
                            reinterpret_cast<VkBufferCreateInfo*>(&ci),
                            &allocInfo,
                            reinterpret_cast<VkBuffer*>(&buffer),
                            &allocation,
                            nullptr);
        }

        void free()
        {
            unmap();
            vmaDestroyBuffer(*allocator, buffer, allocation);
        }

        void* map()
        {
            if (mapped == nullptr)
                vmaMapMemory(*allocator, allocation, &mapped);
            return mapped;
        }

        void unmap()
        {
            if (mapped != nullptr)
                vmaUnmapMemory(*allocator, allocation);
            mapped = nullptr;
        }

        vk::Buffer get_buffer() const { return buffer; }

        vk::DescriptorBufferInfo get_desc_info() const
        {
            vk::DescriptorBufferInfo dbi{};
            dbi.buffer = buffer;
            dbi.offset = 0;
            dbi.range = size;
            return dbi;
        }

        size_t get_size() const { return size; }

        vk::BufferUsageFlags get_buf_usage() const { return buf_usage; }

        VmaMemoryUsage get_mem_usage() const { return mem_usage; }

        private:
        VmaAllocator* allocator;
        void* mapped;
        size_t size;
        vk::BufferUsageFlags buf_usage;
        VmaMemoryUsage mem_usage;
        vk::Buffer buffer;
        VmaAllocation allocation;
    };
}    // namespace my_app
