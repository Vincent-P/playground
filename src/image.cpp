#include "image.hpp"

#include "tools.hpp"
#include "vulkan_context.hpp"

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

namespace my_app
{
    Image::Image()
        : vulkan(nullptr)
        , image_info()
        , mem_usage()
        , image()
        , default_view()
        , default_sampler()
        , allocation()
    {}


    Image::Image(const VulkanContext& _vulkan, vk::ImageCreateInfo _image_info, const char* _name, VmaMemoryUsage _mem_usage)
        : vulkan(&_vulkan)
        , image_info(_image_info)
        , mem_usage(_mem_usage)
        , image()
        , default_view()
        , default_sampler()
        , allocation()
    {
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = mem_usage;

        allocInfo.flags = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
        allocInfo.pUserData = const_cast<void*>(reinterpret_cast<const void*>(_name));

        VK_CHECK(vmaCreateImage(vulkan->allocator,
                                reinterpret_cast<VkImageCreateInfo*>(&image_info),
                                &allocInfo,
                                reinterpret_cast<VkImage*>(&image),
                                &allocation,
                                nullptr));

        vk::DebugUtilsObjectNameInfoEXT debug_name;
        debug_name.pObjectName = _name;
        debug_name.objectType = vk::ObjectType::eImage;
        debug_name.objectHandle = get_raw_vulkan_handle(image);
        _vulkan.device->setDebugUtilsObjectNameEXT(debug_name, _vulkan.dldi);


        vk::ImageViewCreateInfo vci{};
        vci.flags = {};
        vci.image = image;
        vci.format = _image_info.format;
        vci.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
        vci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        vci.subresourceRange.baseMipLevel = 0;
        vci.subresourceRange.levelCount = _image_info.mipLevels;
        vci.subresourceRange.baseArrayLayer = 0;
        vci.subresourceRange.layerCount = _image_info.arrayLayers;
        vci.viewType = view_type_from(_image_info.imageType);

        if (_image_info.usage & vk::ImageUsageFlagBits::eDepthStencilAttachment)
        {
            vci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
        }

        default_view = _vulkan.device->createImageView(vci);


        vk::SamplerCreateInfo sci{};
        sci.magFilter = vk::Filter::eNearest;
        sci.minFilter = vk::Filter::eLinear;
        sci.mipmapMode = vk::SamplerMipmapMode::eLinear;
        sci.addressModeU = vk::SamplerAddressMode::eRepeat;
        sci.addressModeV = vk::SamplerAddressMode::eRepeat;
        sci.addressModeW = vk::SamplerAddressMode::eRepeat;
        sci.compareOp = vk::CompareOp::eNever;
        sci.borderColor = vk::BorderColor::eFloatOpaqueWhite;
        sci.minLod = 0;
        sci.maxLod = float(_image_info.mipLevels);
        sci.maxAnisotropy = 8.0f;
        sci.anisotropyEnable = VK_TRUE;
        default_sampler = vulkan->device->createSampler(sci);
    }

    Image::Image(const VulkanContext& _vulkan, vk::ImageCreateInfo _image_info, VmaMemoryUsage _mem_usage)
    : Image(_vulkan, _image_info, "Image without name", _mem_usage)
    {}

    void Image::free()
    {
        if (image)
        {
            vmaDestroyImage(vulkan->allocator, image, allocation);
            vulkan->device->destroy(default_view);
            vulkan->device->destroy(default_sampler);
        }

    }
}    // namespace my_app
