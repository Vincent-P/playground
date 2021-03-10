#pragma once
#include "base/types.hpp"
#include "base/option.hpp"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace platform
{
    struct Window;
}

namespace vulkan
{
struct Display
{
};

struct Context
{
    VkInstance instance;
    Option<VkDebugUtilsMessengerEXT> debug_messenger;

    VkPhysicalDevice physical_device;
    VkPhysicalDeviceProperties physical_props;

    VkPhysicalDeviceVulkan12Features vulkan12_features;
    VkPhysicalDeviceFeatures2 physical_device_features;

    VkDevice device;
    VmaAllocator allocator;

    u32 graphics_family_idx;
    u32 present_family_idx;

    VkDescriptorPool descriptor_pool;

    static Context create(const platform::Window *window);
    void create_swapchain();
    void create_frame_resources(usize count = 1);
    void destroy_swapchain();
    void on_resize();
    void destroy();


    // Instance functions
#define X(name) PFN_##name name
    X(vkCreateDebugUtilsMessengerEXT);
    X(vkDestroyDebugUtilsMessengerEXT);
    X(vkCmdBeginDebugUtilsLabelEXT);
    X(vkCmdEndDebugUtilsLabelEXT);
    X(vkSetDebugUtilsObjectNameEXT);
#undef X
};

struct GraphicsContext
{
    Context base;

    // graphics only
    VkSurfaceKHR surface;
    Option<Display> display;
    FrameResources frame_resources;
    usize frame_count{0};

};

struct Device
{
};
}
