#pragma once
#include "exo/collections/dynamic_array.h"
#include "exo/collections/enum_array.h"
#include "exo/collections/handle.h"
#include "exo/maths/numerics.h"
#include "rhi/queues.h"
#include <vulkan/vulkan_core.h>

namespace rhi
{
struct Context;
struct Image;

inline constexpr usize MAX_SWAPCHAIN_IMAGES = 6;

struct Surface
{
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;

	exo::EnumArray<VkBool32, QueueType> present_queue_supported = {};
	VkSurfaceFormatKHR format = {};
	VkPresentModeKHR present_mode = {};
	i32 width = 1;
	i32 height = 1;
	u32 previous_image = 0;
	u32 current_image = 0;
	exo::DynamicArray<Handle<Image>, MAX_SWAPCHAIN_IMAGES> images;
	exo::DynamicArray<VkSemaphore, MAX_SWAPCHAIN_IMAGES> image_acquired_semaphores;
	exo::DynamicArray<VkSemaphore, MAX_SWAPCHAIN_IMAGES> can_present_semaphores;

	// --
	static Surface create(Context *context, u64 display_handle, u64 window_handle);
	void destroy(Context *context);

	void resize(Context *ctx);
};
} // namespace rhi
