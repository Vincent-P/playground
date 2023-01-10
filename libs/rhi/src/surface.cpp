#include "exo/memory/dynamic_buffer.h"
#include <vulkan/vulkan_core.h>
#define VK_USE_PLATFORM_WIN32_KHR

#include "exo/collections/dynamic_array.h"
#include "exo/format.h"
#include "exo/memory/scope_stack.h"
#include "rhi/context.h"
#include "rhi/image.h"
#include "rhi/surface.h"
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#undef CreateSemaphore

namespace rhi
{
inline constexpr VkImageUsageFlags SWAPCHAIN_IMAGE_USAGE = color_attachment_usage;

static void create_swapchain(Context *ctx, Surface *surface, VkSwapchainKHR previous_swapchain = VK_NULL_HANDLE)
{
	// Use default extent for the swapchain
	VkSurfaceCapabilitiesKHR capabilities = {};
	ctx->vk.GetPhysicalDeviceSurfaceCapabilitiesKHR(ctx->physical_device, surface->surface, &capabilities);
	if (capabilities.currentExtent.width == 0 || capabilities.currentExtent.height == 0) {
		return;
	}

	surface->width = static_cast<i32>(capabilities.currentExtent.width);
	surface->height = static_cast<i32>(capabilities.currentExtent.height);

	auto image_count = capabilities.minImageCount;
	if (image_count < 3) {
		image_count = image_count + 1u;
	}
	if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
		image_count = capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR create_info = {.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
	create_info.surface = surface->surface;
	create_info.minImageCount = image_count;
	create_info.imageFormat = surface->format.format;
	create_info.imageColorSpace = surface->format.colorSpace;
	create_info.imageExtent.width = static_cast<u32>(surface->width);
	create_info.imageExtent.height = static_cast<u32>(surface->height);
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = SWAPCHAIN_IMAGE_USAGE;
	create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_info.queueFamilyIndexCount = 0;
	create_info.pQueueFamilyIndices = nullptr;
	create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode = surface->present_mode;
	create_info.clipped = VK_TRUE;
	create_info.oldSwapchain = previous_swapchain;

	ctx->vkdevice.CreateSwapchainKHR(ctx->device, &create_info, nullptr, &surface->swapchain);
}

static void create_resources(Context *ctx, Surface *surface)
{
	uint images_count = 0;
	exo::DynamicArray<VkImage, MAX_SWAPCHAIN_IMAGES> vkimages;
	ctx->vkdevice.GetSwapchainImagesKHR(ctx->device, surface->swapchain, &images_count, nullptr);
	vkimages.resize(images_count);
	ctx->vkdevice.GetSwapchainImagesKHR(ctx->device, surface->swapchain, &images_count, vkimages.data());

	exo::ScopeStack scope;
	ASSERT(surface->images.is_empty());
	surface->images.resize(images_count);
	auto image_desc = ImageDescription{
		.size = {surface->width, surface->height, 1},
		.format = surface->format.format,
		.usages = SWAPCHAIN_IMAGE_USAGE,
	};
	for (uint i_image = 0; i_image < images_count; i_image++) {
		image_desc.name = exo::formatf(scope, "Swapchain #%u", i_image),
		surface->images[i_image] = ctx->create_image(&image_desc, vkimages[i_image]);
	}

	ASSERT(surface->can_present_semaphores.is_empty());
	surface->can_present_semaphores.resize(images_count);
	for (auto &semaphore : surface->can_present_semaphores) {
		const VkSemaphoreCreateInfo semaphore_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
		ctx->vkdevice.CreateSemaphore(ctx->device, &semaphore_info, nullptr, &semaphore);
	}

	ASSERT(surface->image_acquired_semaphores.is_empty());
	surface->image_acquired_semaphores.resize(images_count);
	for (auto &semaphore : surface->image_acquired_semaphores) {
		const VkSemaphoreCreateInfo semaphore_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
		ctx->vkdevice.CreateSemaphore(ctx->device, &semaphore_info, nullptr, &semaphore);
	}
}

static void destroy_swapchain(Context *ctx, VkSwapchainKHR swapchain)
{
	ctx->vkdevice.DestroySwapchainKHR(ctx->device, swapchain, nullptr);
}

static void destroy_resources(Context *ctx, Surface *surface)
{
	for (auto image : surface->images) {
		ctx->destroy_image(image);
	}

	for (auto &semaphore : surface->image_acquired_semaphores) {
		ctx->vkdevice.DestroySemaphore(ctx->device, semaphore, nullptr);
		semaphore = VK_NULL_HANDLE;
	}

	for (auto &semaphore : surface->can_present_semaphores) {
		ctx->vkdevice.DestroySemaphore(ctx->device, semaphore, nullptr);
		semaphore = VK_NULL_HANDLE;
	}

	surface->images.clear();
	surface->image_acquired_semaphores.clear();
	surface->can_present_semaphores.clear();
}

Surface Surface::create(Context *ctx, u64 display_handle, u64 window_handle)
{
	Surface surface = {};

	/// --- Create the surface
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	VkWin32SurfaceCreateInfoKHR surface_create_info = {.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
	surface_create_info.hwnd = (HWND)(window_handle);
	surface_create_info.hinstance = (HINSTANCE)(display_handle);
	ctx->vk.CreateWin32SurfaceKHR(ctx->instance, &surface_create_info, nullptr, &surface.surface);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	VkXcbSurfaceCreateInfoKHR sci = {.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR};
	sci.connection = (xcb_connection_t *)display_handle;
	sci.window = u32(window_handle);
	vk_check(vkCreateXcbSurfaceKHR(ctx.instance, &sci, nullptr, &surface.surface));
#else
#error "Unknown platform."
#endif

	// Query support, needed before present
	ctx->vk.GetPhysicalDeviceSurfaceSupportKHR(ctx->physical_device,
		ctx->graphics_family_idx,
		surface.surface,
		&surface.present_queue_supported[QueueType::Graphics]);

	ctx->vk.GetPhysicalDeviceSurfaceSupportKHR(ctx->physical_device,
		ctx->compute_family_idx,
		surface.surface,
		&surface.present_queue_supported[QueueType::Compute]);

	ctx->vk.GetPhysicalDeviceSurfaceSupportKHR(ctx->physical_device,
		ctx->transfer_family_idx,
		surface.surface,
		&surface.present_queue_supported[QueueType::Transfer]);

	// Find a good present mode (by priority Mailbox then Immediate then FIFO)
	u32 present_modes_count = 0;
	ctx->vk.GetPhysicalDeviceSurfacePresentModesKHR(ctx->physical_device,
		surface.surface,
		&present_modes_count,
		nullptr);

	exo::DynamicArray<VkPresentModeKHR, 8> present_modes = {};
	present_modes.resize(present_modes_count);
	ctx->vk.GetPhysicalDeviceSurfacePresentModesKHR(ctx->physical_device,
		surface.surface,
		&present_modes_count,
		present_modes.data());
	surface.present_mode = VK_PRESENT_MODE_FIFO_KHR;

	for (auto &pm : present_modes) {
		if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
			surface.present_mode = pm;
			break;
		}
	}

	if (surface.present_mode == VK_PRESENT_MODE_FIFO_KHR) {
		for (auto &pm : present_modes) {
			if (pm == VK_PRESENT_MODE_IMMEDIATE_KHR) {
				surface.present_mode = pm;
				break;
			}
		}
	}

	// Find the best format
	uint formats_count = 0;
	exo::DynamicArray<VkSurfaceFormatKHR, 32> formats = {};
	ctx->vk.GetPhysicalDeviceSurfaceFormatsKHR(ctx->physical_device, surface.surface, &formats_count, nullptr);
	formats.resize(formats_count);
	ctx->vk.GetPhysicalDeviceSurfaceFormatsKHR(ctx->physical_device, surface.surface, &formats_count, formats.data());
	surface.format = formats[0];
	if (surface.format.format == VK_FORMAT_UNDEFINED) {
		surface.format.format = VK_FORMAT_B8G8R8A8_UNORM;
		surface.format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	} else {
		for (const auto &f : formats) {
			if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				surface.format = f;
				break;
			}
		}
	}

	create_swapchain(ctx, &surface);
	create_resources(ctx, &surface);

	return surface;
}

void Surface::destroy(Context *ctx)
{
	destroy_resources(ctx, this);
	destroy_swapchain(ctx, this->swapchain);
	ctx->vk.DestroySurfaceKHR(ctx->instance, this->surface, nullptr);
}

void Surface::resize(Context *ctx)
{
	auto old_swapchain = this->swapchain;
	destroy_resources(ctx, this);
	create_swapchain(ctx, this);
	destroy_swapchain(ctx, old_swapchain);
	create_resources(ctx, this);
}
} // namespace rhi
