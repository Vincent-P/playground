#include <vulkan/vulkan.hpp>
#include "renderer/hl_api.hpp"
#include <iostream>

namespace my_app::vulkan
{
    RenderTargetH API::create_rendertarget(const RTInfo& info)
    {
        RenderTarget rt;
        rt.is_swapchain = info.is_swapchain;

        rendertargets.push_back(std::move(rt));

        u32 h = static_cast<u32>(rendertargets.size()) - 1;
        return RenderTargetH(h);
    }

    RenderTarget& API::get_rendertarget(RenderTargetH H)
    {
        return rendertargets[H.value()];
    }

    static vk::ImageViewType view_type_from(vk::ImageType _type)
    {
	switch (_type)
	{
	case vk::ImageType::e1D: return vk::ImageViewType::e1D;
	case vk::ImageType::e2D: return vk::ImageViewType::e2D;
	case vk::ImageType::e3D: return vk::ImageViewType::e3D;
	}
	return vk::ImageViewType::e2D;
    }

    ImageH API::create_image(const ImageInfo& info)
    {
	Image img;

        img.image_info.imageType = info.type;
        img.image_info.format = info.format;
        img.image_info.extent.width = info.width;
        img.image_info.extent.height = info.height;
        img.image_info.extent.depth = info.depth;
        img.image_info.mipLevels = info.mip_levels;
        img.image_info.arrayLayers = info.layers;
        img.image_info.samples = info.samples;
        img.image_info.initialLayout = vk::ImageLayout::eUndefined;
        img.image_info.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
        img.image_info.queueFamilyIndexCount = 0;
        img.image_info.pQueueFamilyIndices = nullptr;
        img.image_info.sharingMode = vk::SharingMode::eExclusive;

        VmaAllocationCreateInfo alloc_info{};
        alloc_info.flags = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
        alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        alloc_info.pUserData = const_cast<void*>(reinterpret_cast<const void*>(info.name));

        VK_CHECK(vmaCreateImage(ctx.allocator,
                                reinterpret_cast<VkImageCreateInfo*>(&img.image_info),
                                &alloc_info,
                                reinterpret_cast<VkImage*>(&img.vkhandle),
                                &img.allocation,
                                nullptr));

        vk::DebugUtilsObjectNameInfoEXT debug_name;
        debug_name.pObjectName = info.name;
        debug_name.objectType = vk::ObjectType::eImage;
        debug_name.objectHandle = get_raw_vulkan_handle(img.vkhandle);
        ctx.device->setDebugUtilsObjectNameEXT(debug_name);


        vk::ImageViewCreateInfo vci{};
        vci.flags = {};
        vci.image = img.vkhandle;
        vci.format = img.image_info.format;
        vci.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
        vci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        vci.subresourceRange.baseMipLevel = 0;
        vci.subresourceRange.levelCount = img.image_info.mipLevels;
        vci.subresourceRange.baseArrayLayer = 0;
        vci.subresourceRange.layerCount = img.image_info.arrayLayers;
        vci.viewType = view_type_from(img.image_info.imageType);

        if (img.image_info.usage & vk::ImageUsageFlagBits::eDepthStencilAttachment)
        {
            vci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
        }

        img.default_view = ctx.device->createImageView(vci);


	images.push_back(std::move(img));
        return ImageH(static_cast<u32>(images.size()) - 1);
    }

    Image& API::get_image(ImageH H)
    {
        return images[H.value()];
    }

    static void destroy_image_internal(API& api, Image& img)
    {
	vmaDestroyImage(api.ctx.allocator, img.vkhandle, img.allocation);
	api.ctx.device->destroy(img.default_view);
    }

    void API::destroy_image(ImageH H)
    {
	Image& img = get_image(H);
	destroy_image_internal(*this, img);
    }
}
