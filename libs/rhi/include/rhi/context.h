#pragma once
#include "exo/collections/dynamic_array.h"
#include "exo/collections/pool.h"
#include "exo/option.h"
#include <vulkan/vulkan_core.h>

/*
  The RHI is mostly platform independent and work wherever Vulkan is supported.
  The platform specific bits are:
  - Surface creation: platform-specific handles are passed as a pair of uint64_t and should be interpreted differently
  based on the current platform.
  - Vulkan initialization: at least on Windows Vulkan is a dll that has to be loaded manually with a LoadLibrary
  syscall. The platform layout should provide functions to (un)load a dynamic module, and load function pointers from
  it.
*/

struct Platform;
// from vk_mem_alloc.h
typedef struct VmaAllocator_T *VmaAllocator;
// from vulkan_win32.h
struct VkWin32SurfaceCreateInfoKHR;
typedef VkResult(VKAPI_PTR *PFN_vkCreateWin32SurfaceKHR)(VkInstance instance,
	const VkWin32SurfaceCreateInfoKHR *pCreateInfo,
	const VkAllocationCallbacks *pAllocator,
	VkSurfaceKHR *pSurface);

namespace rhi
{
struct Image;
struct ImageDescription;

struct ContextDescription
{
	bool enable_validation = true;
	bool enable_graphic_windows = true;
};

struct VkInstanceFuncs
{
	void *vulkan_module;
	PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
	PFN_vkGetDeviceProcAddr GetDeviceProcAddr;
	PFN_vkEnumerateInstanceLayerProperties EnumerateInstanceLayerProperties;
	PFN_vkCreateInstance CreateInstance;
	PFN_vkDestroyInstance DestroyInstance;
	PFN_vkCreateDevice CreateDevice;
	PFN_vkDestroyDevice DestroyDevice;
	PFN_vkCreateDebugUtilsMessengerEXT CreateDebugUtilsMessengerEXT;
	PFN_vkDestroyDebugUtilsMessengerEXT DestroyDebugUtilsMessengerEXT;
	PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices;
	PFN_vkGetPhysicalDeviceQueueFamilyProperties GetPhysicalDeviceQueueFamilyProperties;
	PFN_vkGetPhysicalDeviceProperties GetPhysicalDeviceProperties;
	PFN_vkGetPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties;
	PFN_vkGetPhysicalDeviceMemoryProperties2 GetPhysicalDeviceMemoryProperties2;
	PFN_vkCreateWin32SurfaceKHR CreateWin32SurfaceKHR;
	PFN_vkDestroySurfaceKHR DestroySurfaceKHR;
	PFN_vkGetPhysicalDeviceSurfaceSupportKHR GetPhysicalDeviceSurfaceSupportKHR;
	PFN_vkGetPhysicalDeviceSurfacePresentModesKHR GetPhysicalDeviceSurfacePresentModesKHR;
	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR GetPhysicalDeviceSurfaceFormatsKHR;
	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR GetPhysicalDeviceSurfaceCapabilitiesKHR;
};

struct VkDeviceFuncs
{
	PFN_vkAllocateMemory AllocateMemory;
	PFN_vkFreeMemory FreeMemory;
	PFN_vkMapMemory MapMemory;
	PFN_vkUnmapMemory UnmapMemory;
	PFN_vkFlushMappedMemoryRanges FlushMappedMemoryRanges;
	PFN_vkInvalidateMappedMemoryRanges InvalidateMappedMemoryRanges;
	PFN_vkBindBufferMemory BindBufferMemory;
	PFN_vkBindImageMemory BindImageMemory;
	PFN_vkGetBufferMemoryRequirements GetBufferMemoryRequirements;
	PFN_vkGetImageMemoryRequirements GetImageMemoryRequirements;
	PFN_vkCreateBuffer CreateBuffer;
	PFN_vkDestroyBuffer DestroyBuffer;
	PFN_vkCreateImage CreateImage;
	PFN_vkDestroyImage DestroyImage;
	PFN_vkCmdCopyBuffer CmdCopyBuffer;
	PFN_vkGetBufferMemoryRequirements2 GetBufferMemoryRequirements2;
	PFN_vkGetImageMemoryRequirements2 GetImageMemoryRequirements2;
	PFN_vkBindBufferMemory2 BindBufferMemory2;
	PFN_vkBindImageMemory2 BindImageMemory2;
	PFN_vkGetDeviceBufferMemoryRequirementsKHR GetDeviceBufferMemoryRequirementsKHR;
	PFN_vkGetDeviceImageMemoryRequirementsKHR GetDeviceImageMemoryRequirementsKHR;
	PFN_vkCreateSwapchainKHR CreateSwapchainKHR;
	PFN_vkDestroySwapchainKHR DestroySwapchainKHR;
	PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR;
	PFN_vkCreateSemaphore CreateSemaphore;
	PFN_vkDestroySemaphore DestroySemaphore;
	PFN_vkSetDebugUtilsObjectNameEXT SetDebugUtilsObjectNameEXT;
	PFN_vkCreateImageView CreateImageView;
	PFN_vkDestroyImageView DestroyImageView;
};

struct Context
{
	// Vulkan instance
	VkInstance instance;
	Option<VkDebugUtilsMessengerEXT> debug_messenger;

	// Vulkan device
	VkPhysicalDevice physical_device;
	VkDevice device;
	VmaAllocator allocator;
	u32 graphics_family_idx = u32_invalid;
	u32 compute_family_idx = u32_invalid;
	u32 transfer_family_idx = u32_invalid;

	// Resources
	exo::Pool<Image> images;

	// Keep functions pointers at the end
	VkInstanceFuncs vk;
	VkDeviceFuncs vkdevice;

	/// --

	static Context create(Platform *platform, const ContextDescription &desc);
	void destroy(Platform *platform);

	// Resources
	Handle<Image> create_image(const ImageDescription *desc);
	Handle<Image> create_image(const ImageDescription *desc, VkImage proxy);
	void destroy_image(Handle<Image> image);
};
} // namespace rhi
