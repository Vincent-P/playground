#include "render/vulkan/resources.hpp"
#include "render/vulkan/device.hpp"
#include "render/vulkan/utils.hpp"
#include "vulkan/vulkan_core.h"


namespace vulkan
{
Handle<Image> Device::create_image(const ImageDescription &image_desc, Option<VkImage> proxy)
{
    VkImageCreateInfo image_info     = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image_info.imageType             = image_desc.type;
    image_info.format                = image_desc.format;
    image_info.extent.width          = image_desc.size.x;
    image_info.extent.height         = image_desc.size.y;
    image_info.extent.depth          = image_desc.size.z;
    image_info.mipLevels             = 1;
    image_info.arrayLayers           = 1;
    image_info.samples               = image_desc.samples;
    image_info.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage                 = image_desc.usages;
    image_info.queueFamilyIndexCount = 0;
    image_info.pQueueFamilyIndices   = nullptr;
    image_info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    image_info.tiling                = VK_IMAGE_TILING_OPTIMAL;

    VkImageSubresourceRange full_range = {};
    full_range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    full_range.baseMipLevel   = 0;
    full_range.levelCount     = image_info.mipLevels;
    full_range.baseArrayLayer = 0;
    full_range.layerCount     = image_info.arrayLayers;


    VkImage vkhandle = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;

    if (proxy)
    {
        vkhandle = *proxy;
    }
    else
    {
        VmaAllocationCreateInfo alloc_info{};
        alloc_info.flags     = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
        alloc_info.usage     = image_desc.memory_usage;
        alloc_info.pUserData = const_cast<void *>(reinterpret_cast<const void *>(image_desc.name.c_str()));

        VK_CHECK(vmaCreateImage(allocator,
                                reinterpret_cast<VkImageCreateInfo *>(&image_info),
                                &alloc_info,
                                reinterpret_cast<VkImage *>(&vkhandle),
                                &allocation,
                                nullptr));
    }

    VkImageView full_view = VK_NULL_HANDLE;
    VkImageViewCreateInfo vci = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vci.flags                 = 0;
    vci.image                 = vkhandle;
    vci.format                = image_desc.format;
    vci.components.r          = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.g          = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.b          = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.a          = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.subresourceRange      = full_range;
    vci.viewType              = VK_IMAGE_VIEW_TYPE_2D;

    VK_CHECK(vkCreateImageView(device, &vci, nullptr, &full_view));


    return images.add({
            .desc = image_desc,
            .vkhandle = vkhandle,
            .allocation = allocation,
            .usage = ImageUsage::None,
            .is_proxy = proxy.has_value(),
            .full_range = full_range,
            .full_view = full_view,
        });
}

void Device::destroy_image(Handle<Image> image_handle)
{
    if (auto *image = images.get(image_handle))
    {
        if (!image->is_proxy)
        {
            vmaDestroyImage(allocator, image->vkhandle, image->allocation);
        }
        vkDestroyImageView(device, image->full_view, nullptr);
        images.remove(image_handle);
    }
}

uint3 Device::get_image_size(Handle<Image> image_handle)
{
    if (auto *image = images.get(image_handle))
    {
        return image->desc.size;
    }
    return {};
}

};
