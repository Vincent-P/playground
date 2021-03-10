#pragma once
#include "base/types.hpp"
#include "base/vector.hpp"
#include "base/handle.hpp"

#include <vulkan/vulkan.h>

namespace platform { struct Window; }

namespace vulkan
{
struct Context;
struct Device;
struct Image;

struct Surface
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;

    VkSurfaceFormatKHR format;
    VkPresentModeKHR present_mode;
    VkExtent2D extent;
    u32 previous_image = 0;
    u32 current_image = 0;
    Vec<Handle<Image>> images;
    Vec<VkSemaphore> image_acquired_semaphores;
    Vec<VkSemaphore> can_present_semaphores;

    static Surface create(Context &context, const platform::Window &window);
    void destroy(Context &context);
    void create_swapchain(Device &device);
    void destroy_swapchain(Device &device);
};
}
