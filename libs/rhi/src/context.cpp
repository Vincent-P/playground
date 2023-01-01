#include "rhi/context.h"

#include "exo/collections/dynamic_array.h"
#include "exo/logger.h"
#include "exo/macros/debugbreak.h"
#include "exo/memory/linear_allocator.h"
#include "exo/memory/scope_stack.h"
#include "rhi/operators.h"
#include "rhi/utils.h"
#include <vulkan/vulkan_core.h>

namespace rhi
{
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
	VkDebugUtilsMessageTypeFlagsEXT /*message_type*/,
	const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
	void * /*unused*/)
{
	// Read-after-write on bindless render targets
	if (pCallbackData->messageIdNumber == 1287084845) {
		return VK_FALSE;
	}

	// Resize with out of date imageExtent
	if (pCallbackData->messageIdNumber == 0x7cd0911d) {
		return VK_FALSE;
	}

	exo::logger::error("%s\n", pCallbackData->pMessage);

	if (pCallbackData->objectCount) {
		exo::logger::error("Objects:\n");
		for (size_t i = 0; i < pCallbackData->objectCount; i++) {
			const auto &object = pCallbackData->pObjects[i];
			exo::logger::error("\t [%zu] %s\n", i, (object.pObjectName ? object.pObjectName : "NoName"));
		}
	}

	if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		DEBUG_BREAK();
	}

	return VK_FALSE;
}

Context Context::create(const ContextDescription &desc)
{
	Context ctx = {};

	/// --- Load the vulkan dynamic libs
	vk_check(volkInitialize());

	/// --- Create Instance
	exo::DynamicArray<const char *, 8> instance_extensions;

	if (desc.enable_graphic_windows) {
		instance_extensions.push(VK_KHR_SURFACE_EXTENSION_NAME);

#if defined(VK_USE_PLATFORM_WIN32_KHR)
		instance_extensions.push(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
		instance_extensions.push(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#else
		ASSERT(false);
#endif
	}

	instance_extensions.push(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	uint layer_props_count = 0;
	vk_check(vkEnumerateInstanceLayerProperties(&layer_props_count, nullptr));

	auto allocator_scope = exo::ScopeStack::with_allocator(&exo::tls_allocator);
	auto *installed_layers =
		reinterpret_cast<VkLayerProperties *>(allocator_scope.allocate(layer_props_count * sizeof(VkLayerProperties)));
	vk_check(vkEnumerateInstanceLayerProperties(&layer_props_count, installed_layers));

	u32 i_validation = u32_invalid;
	for (u32 i_layer = 0; i_layer < layer_props_count; i_layer += 1) {
		if (std::strcmp(installed_layers[i_layer].layerName, "VK_LAYER_KHRONOS_validation") == 0) {
			i_validation = i_layer;
		}
	}

	const bool enable_validation = desc.enable_validation && i_validation != u32_invalid;
	if (desc.enable_validation && i_validation == u32_invalid) {
		exo::logger::info("Validation layers are enabled but the vulkan layer was not found.\n");
	}

	exo::DynamicArray<const char *, 8> instance_layers;
	if (desc.enable_validation && i_validation != u32_invalid) {
		instance_layers.push("VK_LAYER_KHRONOS_validation");
	}

	VkApplicationInfo app_info = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO};
	app_info.pApplicationName = "Multi";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "GoodEngine";
	app_info.engineVersion = VK_MAKE_VERSION(1, 1, 0);
	app_info.apiVersion = VK_API_VERSION_1_2;

	VkInstanceCreateInfo create_info = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
	create_info.pNext = nullptr;
	create_info.flags = 0;
	create_info.pApplicationInfo = &app_info;
	create_info.enabledLayerCount = static_cast<uint32_t>(instance_layers.len());
	create_info.ppEnabledLayerNames = instance_layers.data();
	create_info.enabledExtensionCount = static_cast<uint32_t>(instance_extensions.len());
	create_info.ppEnabledExtensionNames = instance_extensions.data();

	vk_check(vkCreateInstance(&create_info, nullptr, &ctx.instance));
	volkLoadInstance(ctx.instance);

	/// --- Init debug layers
	if (enable_validation) {
		VkDebugUtilsMessengerCreateInfoEXT ci = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
		ci.flags = 0;
		ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
		ci.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
		ci.messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		ci.pfnUserCallback = debug_callback;

		VkDebugUtilsMessengerEXT messenger;
		vk_check(vkCreateDebugUtilsMessengerEXT(ctx.instance, &ci, nullptr, &messenger));
		ctx.debug_messenger = messenger;
	}

	/// --- Enumerate devices
	uint physical_devices_count = 0;
	vk_check(vkEnumeratePhysicalDevices(ctx.instance, &physical_devices_count, nullptr));
	if (physical_devices_count > ctx.physical_devices.capacity()) {
		exo::logger::info("There are %u physical devices, only the first %zu are enabled.\n",
			physical_devices_count,
			ctx.physical_devices.capacity());
	}

	exo::DynamicArray<VkPhysicalDevice, MAX_PHYSICAL_DEVICES> vkphysical_devices;
	vkphysical_devices.resize(physical_devices_count);
	vk_check(vkEnumeratePhysicalDevices(ctx.instance, &physical_devices_count, vkphysical_devices.data()));

	ctx.physical_devices.resize(physical_devices_count);

	for (uint i_device = 0; i_device < physical_devices_count; i_device++) {
		auto &physical_device = ctx.physical_devices[i_device];

		physical_device.vkdevice = vkphysical_devices[i_device];

		vkGetPhysicalDeviceProperties(physical_device.vkdevice, &physical_device.properties);
		physical_device.vulkan12_features = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
		physical_device.features = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
		physical_device.features.pNext = &physical_device.vulkan12_features;
		vkGetPhysicalDeviceFeatures2(physical_device.vkdevice, &physical_device.features);
		physical_device.features.pNext = nullptr;
	}

	return ctx;
}

void Context::destroy()
{
	if (debug_messenger) {
		vkDestroyDebugUtilsMessengerEXT(instance, *debug_messenger, nullptr);
		debug_messenger = std::nullopt;
	}

	vkDestroyInstance(instance, nullptr);
}

// -- Operators for vulkan structs

bool operator==(const VkPipelineShaderStageCreateInfo &a, const VkPipelineShaderStageCreateInfo &b)
{
	return a.flags == b.flags && a.stage == b.stage && a.module == b.module && a.pName == b.pName &&
	       a.pSpecializationInfo == b.pSpecializationInfo;
}

bool operator==(const VkDescriptorBufferInfo &a, const VkDescriptorBufferInfo &b)
{
	return a.buffer == b.buffer && a.offset == b.offset && a.range == b.range;
}

bool operator==(const VkDescriptorImageInfo &a, const VkDescriptorImageInfo &b)
{
	return a.sampler == b.sampler && a.imageView == b.imageView && a.imageLayout == b.imageLayout;
}

bool operator==(const VkExtent3D &a, const VkExtent3D &b)
{
	return a.width == b.width && a.height == b.height && a.depth == b.depth;
}

bool operator==(const VkImageSubresourceRange &a, const VkImageSubresourceRange &b)
{
	return a.aspectMask == b.aspectMask && a.baseMipLevel == b.baseMipLevel && a.levelCount == b.levelCount &&
	       a.baseArrayLayer == b.baseArrayLayer && a.layerCount == b.layerCount;
}

bool operator==(const VkImageCreateInfo &a, const VkImageCreateInfo &b)
{
	bool same = a.queueFamilyIndexCount == b.queueFamilyIndexCount;
	if (!same) {
		return false;
	}

	if (a.pQueueFamilyIndices && b.pQueueFamilyIndices) {
		for (usize i = 0; i < a.queueFamilyIndexCount; i++) {
			if (a.pQueueFamilyIndices[i] != b.pQueueFamilyIndices[i]) {
				return false;
			}
		}
	} else {
		same = a.pQueueFamilyIndices == b.pQueueFamilyIndices;
	}

	return same && a.flags == b.flags && a.imageType == b.imageType && a.format == b.format && a.extent == b.extent &&
	       a.mipLevels == b.mipLevels && a.arrayLayers == b.arrayLayers && a.samples == b.samples &&
	       a.tiling == b.tiling && a.usage == b.usage && a.sharingMode == b.sharingMode &&
	       a.initialLayout == b.initialLayout;
}

bool operator==(const VkComputePipelineCreateInfo &a, const VkComputePipelineCreateInfo &b)
{
	return a.flags == b.flags && a.stage == b.stage && a.layout == b.layout &&
	       a.basePipelineHandle == b.basePipelineHandle && a.basePipelineIndex == b.basePipelineIndex;
}

bool operator==(const VkFramebufferCreateInfo &a, const VkFramebufferCreateInfo &b)
{
	if (a.attachmentCount != b.attachmentCount) {
		return false;
	}

	for (uint i = 0; i < a.attachmentCount; i++) {
		if (a.pAttachments[i] != b.pAttachments[i]) {
			return false;
		}
	}

	return a.flags == b.flags && a.renderPass == b.renderPass && a.width == b.width && a.height == b.height &&
	       a.layers == b.layers;
}

bool operator==(const VkClearValue &a, const VkClearValue &b) { return std::memcmp(&a, &b, sizeof(VkClearValue)) == 0; }
} // namespace rhi
