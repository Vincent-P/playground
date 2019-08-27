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
    {
    }

    Buffer::Buffer(std::string name, const VmaAllocator& _allocator, size_t _size, vk::BufferUsageFlags _buf_usage, VmaMemoryUsage _mem_usage)
        : allocator(&_allocator)
        , mapped(nullptr)
        , size(_size)
        , buf_usage(_buf_usage)
        , mem_usage(_mem_usage)
        , buffer()
        , allocation()
    {
        vk::BufferCreateInfo ci{};
        ci.usage = buf_usage;
        ci.size = size;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = mem_usage;

        allocInfo.flags = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
        allocInfo.pUserData = name.data();

        VK_CHECK(vmaCreateBuffer(*allocator,
                                 reinterpret_cast<VkBufferCreateInfo*>(&ci),
                                 &allocInfo,
                                 reinterpret_cast<VkBuffer*>(&buffer),
                                 &allocation,
                                 nullptr));
    }

    void Buffer::free()
    {
        if (!size)
            return;

        unmap();
        vmaDestroyBuffer(*allocator, buffer, allocation);
    }

    void* Buffer::map()
    {
        if (!size)
            return nullptr;

        if (mapped == nullptr)
            vmaMapMemory(*allocator, allocation, &mapped);
        return mapped;
    }

    void Buffer::unmap()
    {
        if (!size)
            return;

        if (mapped != nullptr)
            vmaUnmapMemory(*allocator, allocation);
        mapped = nullptr;
    }

    void Buffer::flush()
    {
        if (!size)
            return;

        vmaFlushAllocation(*allocator, allocation, 0, size);
    }
}    // namespace my_app
