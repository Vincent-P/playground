#pragma once
#include <exo/maths/numerics.h>
#include <exo/handle.h>
#include <exo/collections/vector.h>
#include <exo/collections/enum_array.h>

#include "render/vulkan/queues.h"

#include <array>
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

    EnumArray<VkBool32, QueueType> present_queue_supported;
    VkSurfaceFormatKHR             format;
    VkPresentModeKHR               present_mode;
    i32                            width          = 1;
    i32                            height         = 1;
    u32                            previous_image = 0;
    u32                            current_image  = 0;
    Vec<Handle<Image>>             images;
    Vec<VkSemaphore>               image_acquired_semaphores;
    Vec<VkSemaphore>               can_present_semaphores;

    static Surface create(Context &context, Device &device, const platform::Window &window);
    void destroy(Context &context, Device &device);
    void create_swapchain(Device &device);
    void destroy_swapchain(Device &device);
};
}
