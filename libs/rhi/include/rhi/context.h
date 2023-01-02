#pragma once
#include "exo/collections/dynamic_array.h"
#include "exo/option.h"
#include "rhi/physical_device.h"
#include <volk.h>

namespace rhi
{
constexpr usize MAX_PHYSICAL_DEVICES = 4;

struct ContextDescription
{
	bool enable_validation = true;
	bool enable_graphic_windows = true;
};

struct VkInstanceFuncs
{
	PFN_vkDestroyDebugUtilsMessengerEXT DestroyDebugUtilsMessengerEXT;
	PFN_vkDestroyInstance DestroyInstance;
};

struct Context
{
	VkInstance instance = VK_NULL_HANDLE;
	VkInstanceFuncs *vk = nullptr;
	Option<VkDebugUtilsMessengerEXT> debug_messenger = {};
	exo::DynamicArray<PhysicalDevice, MAX_PHYSICAL_DEVICES> physical_devices;

	/// --

	static Context create(const ContextDescription &desc);
	void destroy();
};
} // namespace rhi
