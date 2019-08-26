#include "buffer.hpp"
#include "tools.hpp"

namespace my_app
{
    Buffer::Buffer()
        : allocator(nullptr)
        , mapped(nullptr)
        , size(0)
        , buf_usage()
        , mem_usage()
        , buffer()
        , allocation()
        , destroyed(false)
    {
    }

    Buffer::Buffer(const VmaAllocator& _allocator, size_t _size, vk::BufferUsageFlags _buf_usage, VmaMemoryUsage _mem_usage)
        : allocator(&_allocator)
        , mapped(nullptr)
        , size(_size)
        , buf_usage(_buf_usage)
        , mem_usage(_mem_usage)
        , buffer()
        , allocation()
        , destroyed(false)
    {
        vk::BufferCreateInfo ci{};
        ci.usage = buf_usage;
        ci.size = size;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = mem_usage;

        VK_CHECK(vmaCreateBuffer(*allocator,
                                 reinterpret_cast<VkBufferCreateInfo*>(&ci),
                                 &allocInfo,
                                 reinterpret_cast<VkBuffer*>(&buffer),
                                 &allocation,
                                 nullptr));
    }

    Buffer::Buffer(const Buffer& other)
    {
        if (!destroyed)
            free();

        allocator = other.allocator;
        mapped = other.mapped;
        size = other.size;
        buf_usage = other.buf_usage;
        mem_usage = other.mem_usage;
        buffer = other.buffer;
        allocation = other.allocation;
        destroyed = other.destroyed;
    }

    Buffer& Buffer::operator=(const Buffer& other)
    {
        if (!destroyed)
            free();

        allocator = other.allocator;
        mapped = other.mapped;
        size = other.size;
        buf_usage = other.buf_usage;
        mem_usage = other.mem_usage;
        buffer = other.buffer;
        allocation = other.allocation;
        destroyed = other.destroyed;
        return *this;
    }


    Buffer::~Buffer()
    {
        if (!destroyed)
            free();
    }

    void Buffer::free()
    {
        if (destroyed)
            throw std::runtime_error("Attempt to double-free a Buffer.");

        unmap();
        if (allocation)
            vmaDestroyBuffer(*allocator, buffer, allocation);
        destroyed = true;
    }

    void* Buffer::map()
    {
        if (mapped == nullptr)
            vmaMapMemory(*allocator, allocation, &mapped);
        return mapped;
    }

    void Buffer::unmap()
    {
        if (mapped != nullptr && allocation)
            vmaUnmapMemory(*allocator, allocation);
        mapped = nullptr;
    }

    void Buffer::flush()
    {
        vmaFlushAllocation(*allocator, allocation, 0, size);
    }
}    // namespace my_app
