#pragma once
#include <exo/option.h>
#include <exo/collections/vector.h>

#include <volk.h>

namespace exo { struct Window; }

namespace vulkan
{
struct PhysicalDevice;

struct Context
{
    VkInstance instance;
    Option<VkDebugUtilsMessengerEXT> debug_messenger;
    Vec<PhysicalDevice> physical_devices;

    /// --

    static Context create(bool enable_validation = true, const exo::Window *window = nullptr);
    void destroy();
};
}
