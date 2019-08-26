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
        Image();
        Image(std::string name, const VmaAllocator& _allocator, vk::ImageCreateInfo _image_info, VmaMemoryUsage _mem_usage = VMA_MEMORY_USAGE_GPU_ONLY);

        void free();
        vk::Image get_image() const { return image; }
        VmaMemoryUsage get_mem_usage() const { return mem_usage; }

        private:
        const VmaAllocator* allocator;
        vk::ImageCreateInfo image_info;
        vk::Image image;
        VmaMemoryUsage mem_usage;
        VmaAllocation allocation;
    };
}    // namespace my_app
