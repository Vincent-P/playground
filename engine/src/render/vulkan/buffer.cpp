#include "render/vulkan/buffer.h"

#include "render/vulkan/bindless_set.h"
#include "render/vulkan/device.h"
#include "render/vulkan/utils.h"

namespace vulkan
{
Handle<Buffer> Device::create_buffer(const BufferDescription &buffer_desc)
{
    BufferDescription new_buffer = buffer_desc;
    if (new_buffer.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT && !this->desc.buffer_device_address)
    {
        new_buffer.usage ^= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }

    VkBufferCreateInfo buffer_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.usage              = new_buffer.usage;

    buffer_info.size               = new_buffer.size;
    buffer_info.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;
    buffer_info.queueFamilyIndexCount = 0;
    buffer_info.pQueueFamilyIndices   = nullptr;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage     = new_buffer.memory_usage;
    alloc_info.flags     = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
    alloc_info.pUserData = const_cast<void *>(reinterpret_cast<const void *>(new_buffer.name.c_str()));

    VkBuffer vkhandle = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;

    VK_CHECK(vmaCreateBuffer(allocator,
                             &buffer_info,
                             &alloc_info,
                             &vkhandle,
                             &allocation,
                             nullptr));

    if (vkSetDebugUtilsObjectNameEXT)
    {
        VkDebugUtilsObjectNameInfoEXT name_info = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        name_info.objectHandle                  = reinterpret_cast<u64>(vkhandle);
        name_info.objectType                    = VK_OBJECT_TYPE_BUFFER;
        name_info.pObjectName                   = new_buffer.name.c_str();
        VK_CHECK(vkSetDebugUtilsObjectNameEXT(device, &name_info));
    }

    u64  gpu_address = 0;
    if (buffer_info.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    {
        VkBufferDeviceAddressInfo address_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
        address_info.buffer = vkhandle;
        gpu_address = vkGetBufferDeviceAddress(device, &address_info);
    }

    auto handle = buffers.add({
            .desc = new_buffer,
            .vkhandle = vkhandle,
            .allocation = allocation,
            .gpu_address = gpu_address,
        });

    if (buffer_info.usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
    {
        auto &buffer = *buffers.get(handle);
        buffer.descriptor_idx = bind_descriptor(global_sets.storage_buffers, {.buffer = {handle}});
    }

    return handle;
}

void Device::destroy_buffer(Handle<Buffer> buffer_handle)
{
    if (auto *buffer = buffers.get(buffer_handle))
    {
        if (buffer->mapped)
        {
            vmaUnmapMemory(allocator, buffer->allocation);
            buffer->mapped = nullptr;
        }

        vmaDestroyBuffer(allocator, buffer->vkhandle, buffer->allocation);
        buffers.remove(buffer_handle);
    }
}


void *Device::map_buffer(Handle<Buffer> buffer_handle)
{
    auto &buffer = *buffers.get(buffer_handle);
    if (!buffer.mapped)
    {
        VK_CHECK(vmaMapMemory(allocator, buffer.allocation, &buffer.mapped));
    }

    return buffer.mapped;
}

u64 Device::get_buffer_address(Handle<Buffer> buffer_handle)
{
    auto &buffer = *buffers.get(buffer_handle);

    if (buffer.desc.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    {
        VkBufferDeviceAddressInfo address_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
        address_info.buffer = buffer.vkhandle;
        buffer.gpu_address = vkGetBufferDeviceAddress(device, &address_info);
    }

    return buffer.gpu_address;
}

usize Device::get_buffer_size(Handle<Buffer> buffer_handle)
{
    auto &buffer = *buffers.get(buffer_handle);
    return buffer.desc.size;
}

void Device::flush_buffer(Handle<Buffer> buffer_handle)
{
    auto &buffer = *buffers.get(buffer_handle);
    if (buffer.mapped)
    {
        vmaFlushAllocation(allocator, buffer.allocation, 0, buffer.desc.size);
    }
}
u32 Device::get_buffer_storage_index(Handle<Buffer> buffer_handle)
{
    if (auto *buffer = buffers.get(buffer_handle))
    {
        return buffer->descriptor_idx;
    }
    return 0;
}

}
