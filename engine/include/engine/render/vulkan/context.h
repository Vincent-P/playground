#pragma once
#include <exo/option.h>
#include <exo/collections/dynamic_array.h>
#include "engine/render/vulkan/physical_device.h"

#include <volk.h>

namespace exo { struct Window; }

namespace vulkan
{
constexpr usize MAX_PHYSICAL_DEVICES = 4;

struct ContextDescription
{
	bool enable_validation = true;
	bool enable_graphic_windows = true;
};

struct Context
{
    VkInstance instance;
    Option<VkDebugUtilsMessengerEXT> debug_messenger;
	exo::DynamicArray<PhysicalDevice, MAX_PHYSICAL_DEVICES> physical_devices;

    /// --

    static Context create(const ContextDescription &desc);
    void destroy();
};
}
