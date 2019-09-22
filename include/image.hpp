#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace my_app
{
    struct VulkanContext;

    class Image
    {
        public:
        Image();
        Image(const VulkanContext& _vulkan, vk::ImageCreateInfo _image_info, const char* name, VmaMemoryUsage _mem_usage = VMA_MEMORY_USAGE_GPU_ONLY);
        Image(const VulkanContext& _vulkan, vk::ImageCreateInfo _image_info, VmaMemoryUsage _mem_usage = VMA_MEMORY_USAGE_GPU_ONLY);

        void free();
        vk::Image get_image() const { return image; }
        vk::ImageView get_default_view() const { return default_view; }
        vk::Sampler get_default_sampler() const { return default_sampler; }
        VmaMemoryUsage get_mem_usage() const { return mem_usage; }

        vk::ImageSubresourceRange get_range(vk::ImageAspectFlags _aspect) const
        {
            vk::ImageSubresourceRange range;
            range.aspectMask = _aspect;
            range.baseMipLevel = 0;
            range.levelCount = image_info.mipLevels;
            range.baseArrayLayer = 0;
            range.layerCount = image_info.arrayLayers;
            return range;
        }

        private:
        const VulkanContext* vulkan;
        vk::ImageCreateInfo image_info;
        VmaMemoryUsage mem_usage;
        vk::Image image;
        vk::ImageView default_view;
        vk::Sampler default_sampler;
        VmaAllocation allocation;
    };
}    // namespace my_app
