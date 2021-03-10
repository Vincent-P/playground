#pragma once
#include "base/types.hpp"
#include "base/option.hpp"
#include "base/vector.hpp"

#include "render/vulkan/device.hpp"
#include "render/vulkan/surface.hpp"

#include <vulkan/vulkan.h>

namespace platform { struct Window; }

namespace vulkan
{

struct Context
{
    VkInstance instance;
    Option<VkDebugUtilsMessengerEXT> debug_messenger;

    Device device;
    u32 main_device = u32_invalid;
    Option<Surface> surface;

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
