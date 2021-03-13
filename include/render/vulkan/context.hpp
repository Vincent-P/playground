#pragma once
#include "base/types.hpp"
#include "base/option.hpp"
#include "base/vector.hpp"

#include <vulkan/vulkan.h>

namespace platform { struct Window; }

namespace vulkan
{

struct PhysicalDevice
{
    VkPhysicalDevice vkdevice;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceVulkan12Features vulkan12_features;
    VkPhysicalDeviceFeatures2 features;
};

struct Context
{
    VkInstance instance;
    Option<VkDebugUtilsMessengerEXT> debug_messenger;
    Vec<PhysicalDevice> physical_devices;

    /// --

    static Context create(bool enable_validation, const platform::Window *window);
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


}
