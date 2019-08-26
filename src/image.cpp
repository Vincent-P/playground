#include "image.hpp"
#include "tools.hpp"

namespace my_app
{
    Image::Image()
        : allocator(nullptr)
        , image_info()
        , image()
        , mem_usage()
        , allocation()
    {
    }


#ifndef DEBUG_MEMORY_LEAKS
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif

    Image::Image(std::string name, const VmaAllocator& _allocator, vk::ImageCreateInfo _image_info, VmaMemoryUsage _mem_usage)
        : allocator(&_allocator)
        , image_info(_image_info)
        , image()
        , mem_usage(_mem_usage)
        , allocation()
    {

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = mem_usage;

#ifdef DEBUG_MEMORY_LEAKS
        allocInfo.flags = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
        allocInfo.pUserData = name.data();
#endif

        VK_CHECK(vmaCreateImage(*allocator,
                                reinterpret_cast<VkImageCreateInfo*>(&image_info),
                                &allocInfo,
                                reinterpret_cast<VkImage*>(&image),
                                &allocation,
                                nullptr));
    }


#ifndef DEBUG_MEMORY_LEAKS
#pragma clang diagnostic pop
#endif


    void Image::free()
    {
        if (image)
            vmaDestroyImage(*allocator, image, allocation);
    }
}    // namespace my_app
