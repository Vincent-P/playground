#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>
#pragma clang diagnostic pop

namespace my_app
{
    class Buffer
    {
        public:
        Buffer()
            : allocator_(nullptr)
            , mapped_(nullptr)
            , size_(0)
            , buf_usage_()
            , mem_usage_()
        {
        }

        Buffer(VmaAllocator& allocator, size_t size, vk::BufferUsageFlags buf_usage,
               VmaMemoryUsage mem_usage = VMA_MEMORY_USAGE_CPU_TO_GPU)
            : allocator_(&allocator)
            , mapped_(nullptr)
            , size_(size)
            , buf_usage_(buf_usage)
            , mem_usage_(mem_usage)
        {
            vk::BufferCreateInfo ci;
            ci.setUsage(buf_usage);
            ci.setSize(size);

            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = mem_usage;

            vmaCreateBuffer(*allocator_,
                            reinterpret_cast<VkBufferCreateInfo*>(&ci),
                            &allocInfo,
                            reinterpret_cast<VkBuffer*>(&buffer_),
                            &allocation_,
                            nullptr);
        }

        void Free()
        {
            Unmap();
            vmaDestroyBuffer(*allocator_, buffer_, allocation_);
        }

        void* Map()
        {
            if (mapped_ == nullptr)
                vmaMapMemory(*allocator_, allocation_, &mapped_);
            return mapped_;
        }

        void Unmap()
        {
            if (mapped_ != nullptr)
                vmaUnmapMemory(*allocator_, allocation_);
            mapped_ = nullptr;
        }

        vk::Buffer GetBuffer() const { return buffer_; }

        vk::DescriptorBufferInfo GetDescInfo() const
        {
            vk::DescriptorBufferInfo dbi{};
            dbi.buffer = buffer_;
            dbi.offset = 0;
            dbi.range = size_;
            return dbi;
        }

        size_t GetSize() const { return size_; }

        vk::BufferUsageFlags GetBufUsage() const { return buf_usage_; }

        VmaMemoryUsage GetMemUsage() const { return mem_usage_; }

        private:
        VmaAllocator* allocator_;
        void* mapped_;
        size_t size_;
        vk::BufferUsageFlags buf_usage_;
        VmaMemoryUsage mem_usage_;
        vk::Buffer buffer_;
        VmaAllocation allocation_;
    };
}    // namespace my_app
