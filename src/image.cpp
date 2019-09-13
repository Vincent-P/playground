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
    {}


    Image::Image(std::string name, const VmaAllocator& _allocator, vk::ImageCreateInfo _image_info, VmaMemoryUsage _mem_usage)
        : allocator(&_allocator)
        , image_info(_image_info)
        , image()
        , mem_usage(_mem_usage)
        , allocation()
    {

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = mem_usage;

        allocInfo.flags = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
        allocInfo.pUserData = name.data();

        VK_CHECK(vmaCreateImage(*allocator,
                                reinterpret_cast<VkImageCreateInfo*>(&image_info),
                                &allocInfo,
                                reinterpret_cast<VkImage*>(&image),
                                &allocation,
                                nullptr));
    }


    void Image::free()
    {
        if (image)
            vmaDestroyImage(*allocator, image, allocation);
    }
}    // namespace my_app
