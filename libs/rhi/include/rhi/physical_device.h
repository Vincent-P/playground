#pragma once
#include <volk.h>

namespace rhi
{

struct PhysicalDevice
{
	VkPhysicalDevice                 vkdevice;
	VkPhysicalDeviceProperties       properties;
	VkPhysicalDeviceVulkan12Features vulkan12_features;
	VkPhysicalDeviceFeatures2        features;
};

} // namespace rhi
