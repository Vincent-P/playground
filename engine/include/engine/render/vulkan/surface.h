#pragma once
#include <exo/maths/numerics.h>
#include <exo/collections/handle.h>
#include <exo/collections/dynamic_array.h>
#include <exo/collections/enum_array.h>

#include "engine/render/vulkan/queues.h"

#include <vulkan/vulkan.h>

namespace cross { struct Window; }

namespace vulkan
{
struct Context;
struct Device;
struct Image;

inline constexpr usize MAX_SWAPCHAIN_IMAGES = 6;

struct Surface
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;

    EnumArray<VkBool32, QueueType>                    present_queue_supported;
    VkSurfaceFormatKHR                                format;
    VkPresentModeKHR                                  present_mode;
    i32                                               width          = 1;
    i32                                               height         = 1;
    u32                                               previous_image = 0;
    u32                                               current_image  = 0;
    DynamicArray<Handle<Image>, MAX_SWAPCHAIN_IMAGES> images;
    DynamicArray<VkSemaphore, MAX_SWAPCHAIN_IMAGES>   image_acquired_semaphores;
    DynamicArray<VkSemaphore, MAX_SWAPCHAIN_IMAGES>   can_present_semaphores;

    static Surface create(Context &context, Device &device, const cross::Window &window);
    void destroy(Context &context, Device &device);
    void create_swapchain(Device &device);
    void destroy_swapchain(Device &device);
};
}
