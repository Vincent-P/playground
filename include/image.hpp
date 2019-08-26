#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#pragma clang diagnostic pop

namespace my_app
{
    class Image
    {
        public:
        Image()
            : allocator(nullptr)
            , image_info()
            , image()
            , mem_usage()
            , allocation()
        {
        }

        Image(VmaAllocator& _allocator, vk::ImageCreateInfo _image_info, VmaMemoryUsage _mem_usage = VMA_MEMORY_USAGE_GPU_ONLY)
            : allocator(&_allocator)
            , image_info(_image_info)
            , image()
            , mem_usage(_mem_usage)
            , allocation()
        {
            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = mem_usage;

            vmaCreateImage(*allocator,
                           reinterpret_cast<VkImageCreateInfo*>(&image_info),
                           &allocInfo,
                           reinterpret_cast<VkImage*>(&image),
                           &allocation,
                           nullptr);
        }

        void free() { vmaDestroyImage(*allocator, image, allocation); }

        vk::Image get_image() const { return image; }

        VmaMemoryUsage get_mem_usage() const { return mem_usage; }

        private:
        VmaAllocator* allocator;
        vk::ImageCreateInfo image_info;
        vk::Image image;
        VmaMemoryUsage mem_usage;
        VmaAllocation allocation;
    };
}    // namespace my_app
