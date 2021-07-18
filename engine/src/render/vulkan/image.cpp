#include "render/vulkan/bindless_set.h"
#include "render/vulkan/resources.h"
#include "render/vulkan/device.h"
#include "render/vulkan/utils.h"
#include "vulkan/vulkan_core.h"

namespace vulkan
{

static ImageView create_image_view(Device &device, VkImage vkhandle, std::string &&name, VkImageSubresourceRange &range, VkFormat format, VkImageViewType type)
{
    ImageView view;

    view.vkhandle = VK_NULL_HANDLE;
    view.name = std::move(name);
    view.range = range;

    VkImageViewCreateInfo vci = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vci.flags                 = 0;
    vci.image                 = vkhandle;
    vci.format                = format;
    vci.components.r          = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.g          = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.b          = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.a          = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.subresourceRange      = range;
    vci.viewType              = type;

    VK_CHECK(vkCreateImageView(device.device, &vci, nullptr, &view.vkhandle));

    if (device.vkSetDebugUtilsObjectNameEXT)
    {
        VkDebugUtilsObjectNameInfoEXT ni = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        ni.objectHandle                  = reinterpret_cast<u64>(view.vkhandle);
        ni.objectType                    = VK_OBJECT_TYPE_IMAGE_VIEW;
        ni.pObjectName                   = view.name.c_str();
        VK_CHECK(device.vkSetDebugUtilsObjectNameEXT(device.device, &ni));
    }

    view.sampled_idx = 0;

    return view;
}

Handle<Image> Device::create_image(const ImageDescription &image_desc, Option<VkImage> proxy)
{
    bool is_sampled = image_desc.usages & VK_IMAGE_USAGE_SAMPLED_BIT;
    bool is_storage = image_desc.usages & VK_IMAGE_USAGE_STORAGE_BIT;
    bool is_depth   = is_depth_format(image_desc.format);

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

    if (this->vkSetDebugUtilsObjectNameEXT)
    {
        VkDebugUtilsObjectNameInfoEXT ni = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        ni.objectHandle                  = reinterpret_cast<u64>(vkhandle);
        ni.objectType                    = VK_OBJECT_TYPE_IMAGE;
        ni.pObjectName                   = image_desc.name.c_str();
        VK_CHECK(this->vkSetDebugUtilsObjectNameEXT(device, &ni));
    }


    VkImageSubresourceRange full_range = {};
    full_range.aspectMask     = is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    full_range.baseMipLevel   = 0;
    full_range.levelCount     = image_info.mipLevels;
    full_range.baseArrayLayer = 0;
    full_range.layerCount     = image_info.arrayLayers;
    VkFormat format = image_desc.format;

    ImageView full_view = create_image_view(*this, vkhandle, fmt::format("{} full view", image_desc.name), full_range, format, view_type_from_image(image_desc.type));

    auto handle = images.add({
            .desc = image_desc,
            .vkhandle = vkhandle,
            .allocation = allocation,
            .usage = ImageUsage::None,
            .is_proxy = proxy.has_value(),
            .full_view = full_view,
        });

    // Bindless (bind everything)
    if (is_sampled)
    {
        auto &image = *images.get(handle);
        image.full_view.sampled_idx = bind_descriptor(global_sets.sampled_images, {.image = {handle}});
    }

    if (is_storage)
    {
        auto &image = *images.get(handle);
        image.full_view.storage_idx = bind_descriptor(global_sets.storage_images, {.image = {handle}});
    }

    return handle;
}

void Device::destroy_image(Handle<Image> image_handle)
{
    if (auto *image = images.get(image_handle))
    {
        if (image->full_view.sampled_idx != u32_invalid) { unbind_descriptor(global_sets.sampled_images, image->full_view.sampled_idx); }
        if (image->full_view.storage_idx != u32_invalid) { unbind_descriptor(global_sets.storage_images, image->full_view.storage_idx); }
        if (!image->is_proxy)
        {
            vmaDestroyImage(allocator, image->vkhandle, image->allocation);
        }
        vkDestroyImageView(device, image->full_view.vkhandle, nullptr);
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

u32 Device::get_image_sampled_index(Handle<Image> image_handle)
{
    if (auto *image = images.get(image_handle))
    {
        return image->full_view.sampled_idx;
    }
    return 0;
}

u32 Device::get_image_storage_index(Handle<Image> image_handle)
{
    if (auto *image = images.get(image_handle))
    {
        return image->full_view.storage_idx;
    }
    return 0;
}
};
