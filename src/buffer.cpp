#include "buffer.hpp"

#include <vulkan/vulkan.hpp>
#include "tools.hpp"
#include "vulkan_context.hpp"

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
    {}

    Buffer::Buffer(const VulkanContext& _vulkan, size_t _size, vk::BufferUsageFlags _buf_usage, const char* _name, VmaMemoryUsage _mem_usage)
        : allocator(&_vulkan.allocator)
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
        allocInfo.pUserData = const_cast<void*>(reinterpret_cast<const void*>(_name));

        VK_CHECK(vmaCreateBuffer(*allocator,
                                 reinterpret_cast<VkBufferCreateInfo*>(&ci),
                                 &allocInfo,
                                 reinterpret_cast<VkBuffer*>(&buffer),
                                 &allocation,
                                 nullptr));

        vk::DebugUtilsObjectNameInfoEXT debug_name;
        debug_name.pObjectName = _name;
        debug_name.objectType = vk::ObjectType::eBuffer;
        debug_name.objectHandle = get_raw_vulkan_handle(buffer);
        _vulkan.device->setDebugUtilsObjectNameEXT(debug_name, _vulkan.dldi);
    }

    Buffer::Buffer(const VulkanContext& _vulkan, size_t _size, vk::BufferUsageFlags _buf_usage, VmaMemoryUsage _mem_usage)
        : Buffer{_vulkan, _size, _buf_usage, "Buffer without name", _mem_usage}
    {}

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
