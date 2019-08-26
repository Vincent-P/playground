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
        , destroyed(false)
    {
    }

    Image::Image(const VmaAllocator& _allocator, vk::ImageCreateInfo _image_info, VmaMemoryUsage _mem_usage)
        : allocator(&_allocator)
        , image_info(_image_info)
        , image()
        , mem_usage(_mem_usage)
        , allocation()
    {
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = mem_usage;

        VK_CHECK(vmaCreateImage(*allocator,
                                reinterpret_cast<VkImageCreateInfo*>(&image_info),
                                &allocInfo,
                                reinterpret_cast<VkImage*>(&image),
                                &allocation,
                                nullptr));
    }

    Image::Image(const Image& other)
    {
        if (!destroyed)
            free();

        allocator = other.allocator;
        image_info = other.image_info;
        image = other.image;
        mem_usage = other.mem_usage;
        allocation = other.allocation;
        destroyed = other.destroyed;
    }

    Image& Image::operator=(const Image& other)
    {
        if (!destroyed)
            free();

        allocator = other.allocator;
        image_info = other.image_info;
        image = other.image;
        mem_usage = other.mem_usage;
        allocation = other.allocation;
        destroyed = other.destroyed;
        return *this;
    }


    Image::~Image()
    {
        if (!destroyed)
            free();
    }

    void Image::free()
    {
        if (destroyed)
            throw std::runtime_error("Attempt to double-free an Image.");

        if (allocation)
            vmaDestroyImage(*allocator, image, allocation);
        destroyed = true;
    }
}    // namespace my_app
