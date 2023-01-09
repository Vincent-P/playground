#include "rhi/context.h"

#include "exo/collections/dynamic_array.h"
#include "exo/logger.h"
#include "exo/macros/debugbreak.h"
#include "exo/memory/linear_allocator.h"
#include "exo/memory/scope_stack.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#undef CreateSemaphore

/* platform .h */

enum struct PlatformType : uint32_t
{
	Win32,
	Count
};

struct PlatformWindow
{
	uint64_t display_handle;
	uint64_t window_handle;
};

struct GameState;

struct Platform
{
	PlatformType type;
	PlatformWindow *window;
	GameState *game_state;

	void (*debug_print)(const char *);

	using LoadLibraryFn = void *(*)(const char *);
	using GetLibraryProcFn = void *(*)(void *, const char *);
	using UnloadLibraryFn = void (*)(void *);
	LoadLibraryFn load_library;
	GetLibraryProcFn get_library_proc;
	UnloadLibraryFn unload_library;
};
/* platform .h end */

namespace rhi
{
// -- Vulkan loading

static void init_vulkan(Platform *platform, VkInstanceFuncs *funcs)
{
	funcs->vulkan_module = platform->load_library("vulkan-1.dll");

	auto vkGetInstanceProcAddr =
		(PFN_vkGetInstanceProcAddr)platform->get_library_proc(funcs->vulkan_module, "vkGetInstanceProcAddr");
	auto vkGetDeviceProcAddr =
		(PFN_vkGetDeviceProcAddr)platform->get_library_proc(funcs->vulkan_module, "vkGetDeviceProcAddr");

	funcs->GetInstanceProcAddr = vkGetInstanceProcAddr;
	funcs->GetDeviceProcAddr = vkGetDeviceProcAddr;

#define LOAD_INSTANCE_FUN(inst, x) funcs->x = (PFN_vk##x)vkGetInstanceProcAddr(inst, "vk" #x)

	LOAD_INSTANCE_FUN(NULL, EnumerateInstanceLayerProperties);
	LOAD_INSTANCE_FUN(NULL, CreateInstance);
}

static void load_vulkan_instance(VkInstance instance, VkInstanceFuncs *funcs)
{

	auto vkGetInstanceProcAddr = funcs->GetInstanceProcAddr;
	LOAD_INSTANCE_FUN(instance, CreateDevice);
	LOAD_INSTANCE_FUN(instance, DestroyDevice);
	LOAD_INSTANCE_FUN(instance, DestroyInstance);
	LOAD_INSTANCE_FUN(instance, CreateDebugUtilsMessengerEXT);
	LOAD_INSTANCE_FUN(instance, DestroyDebugUtilsMessengerEXT);
	LOAD_INSTANCE_FUN(instance, EnumeratePhysicalDevices);
	LOAD_INSTANCE_FUN(instance, GetPhysicalDeviceQueueFamilyProperties);
	LOAD_INSTANCE_FUN(instance, GetPhysicalDeviceProperties);
	LOAD_INSTANCE_FUN(instance, GetPhysicalDeviceMemoryProperties);
	LOAD_INSTANCE_FUN(instance, GetPhysicalDeviceMemoryProperties2);
	LOAD_INSTANCE_FUN(instance, CreateWin32SurfaceKHR);
	LOAD_INSTANCE_FUN(instance, DestroySurfaceKHR);
	LOAD_INSTANCE_FUN(instance, GetPhysicalDeviceSurfaceSupportKHR);
	LOAD_INSTANCE_FUN(instance, GetPhysicalDeviceSurfacePresentModesKHR);
	LOAD_INSTANCE_FUN(instance, GetPhysicalDeviceSurfaceFormatsKHR);
	LOAD_INSTANCE_FUN(instance, GetPhysicalDeviceSurfaceCapabilitiesKHR);
}
#undef LOAD_INSTANCE_FUN

static void load_vulkan_device(VkDevice device, VkInstanceFuncs *inst_funcs, VkDeviceFuncs *dev_funcs)
{
	auto vkGetDeviceProcAddr = inst_funcs->GetDeviceProcAddr;
#define LOAD_DEVICE_FUN(x) dev_funcs->x = (PFN_vk##x)vkGetDeviceProcAddr(device, "vk" #x)

	LOAD_DEVICE_FUN(AllocateMemory);
	LOAD_DEVICE_FUN(FreeMemory);
	LOAD_DEVICE_FUN(MapMemory);
	LOAD_DEVICE_FUN(UnmapMemory);
	LOAD_DEVICE_FUN(FlushMappedMemoryRanges);
	LOAD_DEVICE_FUN(InvalidateMappedMemoryRanges);
	LOAD_DEVICE_FUN(BindBufferMemory);
	LOAD_DEVICE_FUN(BindImageMemory);
	LOAD_DEVICE_FUN(GetBufferMemoryRequirements);
	LOAD_DEVICE_FUN(GetImageMemoryRequirements);
	LOAD_DEVICE_FUN(CreateBuffer);
	LOAD_DEVICE_FUN(DestroyBuffer);
	LOAD_DEVICE_FUN(CreateImage);
	LOAD_DEVICE_FUN(DestroyImage);
	LOAD_DEVICE_FUN(CmdCopyBuffer);
	LOAD_DEVICE_FUN(GetBufferMemoryRequirements2);
	LOAD_DEVICE_FUN(GetImageMemoryRequirements2);
	LOAD_DEVICE_FUN(BindBufferMemory2);
	LOAD_DEVICE_FUN(BindImageMemory2);
	LOAD_DEVICE_FUN(GetDeviceBufferMemoryRequirementsKHR);
	LOAD_DEVICE_FUN(GetDeviceImageMemoryRequirementsKHR);

	LOAD_DEVICE_FUN(CreateSwapchainKHR);
	LOAD_DEVICE_FUN(DestroySwapchainKHR);
	LOAD_DEVICE_FUN(GetSwapchainImagesKHR);
	LOAD_DEVICE_FUN(CreateSemaphore);
	LOAD_DEVICE_FUN(DestroySemaphore);
	LOAD_DEVICE_FUN(SetDebugUtilsObjectNameEXT);
	LOAD_DEVICE_FUN(CreateImageView);
	LOAD_DEVICE_FUN(DestroyImageView);

#undef LOAD_DEVICE_FUN
}

static void shutdown_vulkan(Platform *platform, VkInstanceFuncs *funcs)
{
	platform->unload_library(funcs->vulkan_module);
	*funcs = {};
}

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

static void create_instance(Platform *platform, Context *ctx, const ContextDescription *desc)
{
	/// --- Load the vulkan dynamic libs
	init_vulkan(platform, &ctx->vk);

	/// --- Create Instance
	exo::DynamicArray<const char *, 8> instance_extensions;

	if (desc->enable_graphic_windows) {
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

	exo::DynamicArray<VkLayerProperties, 64> installed_layers;
	u32 layer_props_count = installed_layers.capacity();
	ctx->vk.EnumerateInstanceLayerProperties(&layer_props_count, nullptr);
	installed_layers.resize(layer_props_count);
	ctx->vk.EnumerateInstanceLayerProperties(&layer_props_count, installed_layers.data());

	u32 i_validation = u32_invalid;
	for (u32 i_layer = 0; i_layer < layer_props_count; i_layer += 1) {
		if (std::strcmp(installed_layers[i_layer].layerName, "VK_LAYER_KHRONOS_validation") == 0) {
			i_validation = i_layer;
		}
	}

	const bool enable_validation = desc->enable_validation && i_validation != u32_invalid;
	if (desc->enable_validation && i_validation == u32_invalid) {
		exo::logger::info("Validation layers are enabled but the vulkan layer was not found.\n");
	}

	exo::DynamicArray<const char *, 8> instance_layers;
	if (desc->enable_validation && i_validation != u32_invalid) {
		instance_layers.push("VK_LAYER_KHRONOS_validation");
	}

	VkApplicationInfo app_info = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO};
	app_info.pApplicationName = "Multi";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "GoodEngine";
	app_info.engineVersion = VK_MAKE_VERSION(1, 1, 0);
	app_info.apiVersion = VK_API_VERSION_1_2;

	VkInstanceCreateInfo instance_create_info = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
	instance_create_info.pNext = nullptr;
	instance_create_info.flags = 0;
	instance_create_info.pApplicationInfo = &app_info;
	instance_create_info.enabledLayerCount = instance_layers.len();
	instance_create_info.ppEnabledLayerNames = instance_layers.data();
	instance_create_info.enabledExtensionCount = instance_extensions.len();
	instance_create_info.ppEnabledExtensionNames = instance_extensions.data();

	ctx->vk.CreateInstance(&instance_create_info, nullptr, &ctx->instance);
	load_vulkan_instance(ctx->instance, &ctx->vk);

	/// --- Init debug layers
	if (enable_validation) {
		VkDebugUtilsMessengerCreateInfoEXT ci = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
		ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
		ci.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
		ci.messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		ci.pfnUserCallback = debug_callback;

		VkDebugUtilsMessengerEXT messenger;
		ctx->vk.CreateDebugUtilsMessengerEXT(ctx->instance, &ci, nullptr, &messenger);
		ctx->debug_messenger = messenger;
	}
}

static void create_device(Context *ctx)
{
	// Enumerate devices
	exo::DynamicArray<VkPhysicalDevice, 4> vkphysical_devices = {};
	uint vkphysical_devices_count = vkphysical_devices.capacity();
	ctx->vk.EnumeratePhysicalDevices(ctx->instance, &vkphysical_devices_count, nullptr);
	vkphysical_devices.resize(vkphysical_devices_count);
	ctx->vk.EnumeratePhysicalDevices(ctx->instance, &vkphysical_devices_count, vkphysical_devices.data());

	// Pick device
	ctx->physical_device = vkphysical_devices[0];

	exo::DynamicArray<const char *, 8> device_extensions = {};
	device_extensions.push(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	// Create queues
	exo::DynamicArray<VkQueueFamilyProperties, 8> queue_families = {};
	uint queue_families_count = queue_families.capacity();
	ctx->vk.GetPhysicalDeviceQueueFamilyProperties(ctx->physical_device, &queue_families_count, nullptr);
	queue_families.resize(queue_families_count);
	ctx->vk.GetPhysicalDeviceQueueFamilyProperties(ctx->physical_device, &queue_families_count, queue_families.data());

	exo::DynamicArray<VkDeviceQueueCreateInfo, 8> queue_create_infos;

	ctx->graphics_family_idx = u32_invalid;
	ctx->compute_family_idx = u32_invalid;
	ctx->transfer_family_idx = u32_invalid;

	const float priority = 0.0;
	for (uint32_t i = 0; i < queue_families.len(); i++) {
		VkDeviceQueueCreateInfo queue_info = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
		queue_info.queueFamilyIndex = i;
		queue_info.queueCount = 1;
		queue_info.pQueuePriorities = &priority;

		if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			if (ctx->graphics_family_idx == u32_invalid) {
				queue_create_infos.push(queue_info);
				ctx->graphics_family_idx = i;
			}
		} else if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
			if (ctx->compute_family_idx == u32_invalid && (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
				queue_create_infos.push(queue_info);
				ctx->compute_family_idx = i;
			}
		} else if (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
			if (ctx->transfer_family_idx == u32_invalid && (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT)) {
				queue_create_infos.push(queue_info);
				ctx->transfer_family_idx = i;
			}
		}
	}

	if (ctx->graphics_family_idx == u32_invalid) {
		exo::logger::error("Failed to find a graphics queue.\n");
	}
	if (ctx->compute_family_idx == u32_invalid) {
		exo::logger::error("Failed to find a compute queue.\n");
	}
	if (ctx->transfer_family_idx == u32_invalid) {
		exo::logger::error("Failed to find a transfer queue.\n");
		ctx->transfer_family_idx = ctx->compute_family_idx;
	}

	VkDeviceCreateInfo device_create_info = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
	device_create_info.pNext = nullptr;
	device_create_info.flags = 0;
	device_create_info.queueCreateInfoCount = queue_create_infos.len();
	device_create_info.pQueueCreateInfos = queue_create_infos.data();
	device_create_info.enabledLayerCount = 0;
	device_create_info.ppEnabledLayerNames = nullptr;
	device_create_info.enabledExtensionCount = device_extensions.len();
	device_create_info.ppEnabledExtensionNames = device_extensions.data();
	device_create_info.pEnabledFeatures = nullptr;

	ctx->vk.CreateDevice(ctx->physical_device, &device_create_info, nullptr, &ctx->device);
	load_vulkan_device(ctx->device, &ctx->vk, &ctx->vkdevice);

	// Create allocator
	VmaVulkanFunctions vma_functions = {};
	vma_functions.vkGetPhysicalDeviceProperties = ctx->vk.GetPhysicalDeviceProperties;
	vma_functions.vkGetPhysicalDeviceMemoryProperties = ctx->vk.GetPhysicalDeviceMemoryProperties;
	vma_functions.vkGetPhysicalDeviceMemoryProperties2KHR = ctx->vk.GetPhysicalDeviceMemoryProperties2;
	vma_functions.vkAllocateMemory = ctx->vkdevice.AllocateMemory;
	vma_functions.vkFreeMemory = ctx->vkdevice.FreeMemory;
	vma_functions.vkMapMemory = ctx->vkdevice.MapMemory;
	vma_functions.vkUnmapMemory = ctx->vkdevice.UnmapMemory;
	vma_functions.vkFlushMappedMemoryRanges = ctx->vkdevice.FlushMappedMemoryRanges;
	vma_functions.vkInvalidateMappedMemoryRanges = ctx->vkdevice.InvalidateMappedMemoryRanges;
	vma_functions.vkBindBufferMemory = ctx->vkdevice.BindBufferMemory;
	vma_functions.vkBindImageMemory = ctx->vkdevice.BindImageMemory;
	vma_functions.vkGetBufferMemoryRequirements = ctx->vkdevice.GetBufferMemoryRequirements;
	vma_functions.vkGetImageMemoryRequirements = ctx->vkdevice.GetImageMemoryRequirements;
	vma_functions.vkCreateBuffer = ctx->vkdevice.CreateBuffer;
	vma_functions.vkDestroyBuffer = ctx->vkdevice.DestroyBuffer;
	vma_functions.vkCreateImage = ctx->vkdevice.CreateImage;
	vma_functions.vkDestroyImage = ctx->vkdevice.DestroyImage;
	vma_functions.vkCmdCopyBuffer = ctx->vkdevice.CmdCopyBuffer;
	vma_functions.vkGetBufferMemoryRequirements2KHR = ctx->vkdevice.GetBufferMemoryRequirements2;
	vma_functions.vkGetImageMemoryRequirements2KHR = ctx->vkdevice.GetImageMemoryRequirements2;
	vma_functions.vkBindBufferMemory2KHR = ctx->vkdevice.BindBufferMemory2;
	vma_functions.vkBindImageMemory2KHR = ctx->vkdevice.BindImageMemory2;
#if 0
	vma_functions.vkGetDeviceBufferMemoryRequirementsKHR = ctx->vkdevice.GetDeviceBufferMemoryRequirementsKHR;
	vma_functions.vkGetDeviceImageMemoryRequirementsKHR = ctx->vkdevice.GetDeviceImageMemoryRequirementsKHR;
#endif

	VmaAllocatorCreateInfo allocator_info = {};
	allocator_info.vulkanApiVersion = VK_API_VERSION_1_2;
	allocator_info.physicalDevice = ctx->physical_device;
	allocator_info.device = ctx->device;
	allocator_info.instance = ctx->instance;
	allocator_info.pVulkanFunctions = &vma_functions;
	vmaCreateAllocator(&allocator_info, &ctx->allocator);
}

static void create_device_resources(Context * /*ctx*/) {}

Context Context::create(Platform *platform, const ContextDescription &desc)
{
	Context ctx = {};
	create_instance(platform, &ctx, &desc);
	create_device(&ctx);
	create_device_resources(&ctx);
	return ctx;
}

void Context::destroy(Platform *platform)
{
	vmaDestroyAllocator(this->allocator);

	this->vk.DestroyDevice(this->device, nullptr);
	if (this->debug_messenger) {
		this->vk.DestroyDebugUtilsMessengerEXT(this->instance, *debug_messenger, nullptr);
		debug_messenger = {};
	}

	this->vk.DestroyInstance(this->instance, nullptr);
	shutdown_vulkan(platform, &this->vk);
}
} // namespace rhi
