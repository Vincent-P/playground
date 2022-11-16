#pragma once
#include "render/vulkan/physical_device.h"
#include "exo/collections/dynamic_array.h"
#include "exo/option.h"

#include <volk.h>

namespace vulkan
{
constexpr usize MAX_PHYSICAL_DEVICES = 4;

struct ContextDescription
{
	bool enable_validation      = true;
	bool enable_graphic_windows = true;
};

struct Context
{
	VkInstance                                              instance        = VK_NULL_HANDLE;
	Option<VkDebugUtilsMessengerEXT>                        debug_messenger = {};
	exo::DynamicArray<PhysicalDevice, MAX_PHYSICAL_DEVICES> physical_devices;

	/// --

	static Context create(const ContextDescription &desc);
	void           destroy();
};
} // namespace vulkan
