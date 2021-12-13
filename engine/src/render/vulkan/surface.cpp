#include "render/vulkan/surface.h"

#include <exo/os/window.h>
#include <exo/collections/dynamic_array.h>
#include "render/vulkan/context.h"
#include "render/vulkan/device.h"
#include "render/vulkan/utils.h"

namespace vulkan
{

Surface Surface::create(Context &context, Device &device, const exo::Window &window)
{
    Surface surface = {};

    /// --- Create the surface
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    VkWin32SurfaceCreateInfoKHR sci = {.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    sci.hwnd                        = window.win32.window;
    sci.hinstance                   = GetModuleHandle(nullptr);
    VK_CHECK(vkCreateWin32SurfaceKHR(context.instance, &sci, nullptr, &surface.surface));
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    VkXcbSurfaceCreateInfoKHR sci = {.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR};
    sci.connection                = window.xcb.connection;
    sci.window                    = window.xcb.window;
    VK_CHECK(vkCreateXcbSurfaceKHR(context.instance, &sci, nullptr, &surface.surface));
#endif

    // Query support, needed before present
    VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device.physical_device.vkdevice, device.graphics_family_idx, surface.surface, &surface.present_queue_supported[QueueType::Graphics]));
    VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device.physical_device.vkdevice, device.compute_family_idx, surface.surface, &surface.present_queue_supported[QueueType::Compute]));
    VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device.physical_device.vkdevice, device.transfer_family_idx, surface.surface, &surface.present_queue_supported[QueueType::Transfer]));

    surface.create_swapchain(device);

    return surface;
}

void Surface::destroy(Context &context, Device &device)
{
    this->destroy_swapchain(device);
    vkDestroySurfaceKHR(context.instance, surface, nullptr);
}

void Surface::create_swapchain(Device &device)
{
    // Use default extent for the swapchain
    VkSurfaceCapabilitiesKHR capabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physical_device.vkdevice, this->surface, &capabilities));
    this->width  = static_cast<i32>(capabilities.currentExtent.width);
    this->height = static_cast<i32>(capabilities.currentExtent.height);

    exo::logger::info("Creating swapchain {}x{}\n", width, height);

    // Find a good present mode (by priority Mailbox then Immediate then FIFO)
    u32 present_modes_count = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device.physical_device.vkdevice, this->surface, &present_modes_count, nullptr));

    exo::DynamicArray<VkPresentModeKHR, 8> present_modes = {};
    present_modes.resize(present_modes_count);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device.physical_device.vkdevice, this->surface, &present_modes_count, present_modes.data()));
    this->present_mode = VK_PRESENT_MODE_FIFO_KHR;

    for (auto &pm : present_modes)
    {
        if (pm == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            this->present_mode = pm;
            break;
        }
    }

    if (this->present_mode == VK_PRESENT_MODE_FIFO_KHR)
    {
        for (auto &pm : present_modes)
        {
            if (pm == VK_PRESENT_MODE_IMMEDIATE_KHR)
            {
                this->present_mode = pm;
                break;
            }
        }
    }

    // Find the best format
    uint                    formats_count = 0;
    Vec<VkSurfaceFormatKHR> formats;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device.physical_device.vkdevice, this->surface, &formats_count, nullptr));
    formats.resize(formats_count);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device.physical_device.vkdevice, this->surface, &formats_count, formats.data()));
    this->format = formats[0];
    if (this->format.format == VK_FORMAT_UNDEFINED)
    {
        this->format.format     = VK_FORMAT_B8G8R8A8_UNORM;
        this->format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    }
    else
    {
        for (const auto &f : formats)
        {
            if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                this->format = f;
                break;
            }
        }
    }

    auto image_count = capabilities.minImageCount;
    if (image_count < 3) {
        image_count = image_count + 1u;
    }
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount)
    {
        image_count = capabilities.maxImageCount;
    }

    VkImageUsageFlags image_usage = color_attachment_usage | storage_image_usage;

    VkSwapchainCreateInfoKHR ci = {.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface                  = this->surface;
    ci.minImageCount            = image_count;
    ci.imageFormat              = this->format.format;
    ci.imageColorSpace          = this->format.colorSpace;
    ci.imageExtent.width        = static_cast<u32>(this->width);
    ci.imageExtent.height       = static_cast<u32>(this->height);
    ci.imageArrayLayers         = 1;
    ci.imageUsage               = image_usage;
    ci.imageSharingMode         = VK_SHARING_MODE_EXCLUSIVE;
    ci.queueFamilyIndexCount    = 0;
    ci.pQueueFamilyIndices      = nullptr;
    ci.preTransform             = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    ci.compositeAlpha           = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode              = this->present_mode;
    ci.clipped                  = VK_TRUE;

    VK_CHECK(vkCreateSwapchainKHR(device.device, &ci, nullptr, &this->swapchain));

    uint images_count = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(device.device, this->swapchain, &images_count, nullptr));

    Vec<VkImage> vkimages(images_count);
    VK_CHECK(vkGetSwapchainImagesKHR(device.device, this->swapchain, &images_count, vkimages.data()));

    this->images.resize(images_count);
    for (uint i_image = 0; i_image < images_count; i_image++)
    {
        this->images[i_image] = device.create_image(
            {
                .name   = fmt::format("Swapchain #{}", i_image),
                .size   = {width, height, 1},
                .format = format.format,
                .usages = image_usage,
            },
            vkimages[i_image]);
    }

    this->can_present_semaphores.resize(images_count);
    for (auto &semaphore : this->can_present_semaphores)
    {
        VkSemaphoreCreateInfo semaphore_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VK_CHECK(vkCreateSemaphore(device.device, &semaphore_info, nullptr, &semaphore));
    }

    this->image_acquired_semaphores.resize(images_count);
    for (auto &semaphore : this->image_acquired_semaphores)
    {
        VkSemaphoreCreateInfo semaphore_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VK_CHECK(vkCreateSemaphore(device.device, &semaphore_info, nullptr, &semaphore));
    }
}

void Surface::destroy_swapchain(Device &device)
{
    for (auto image : images)
    {
        device.destroy_image(image);
    }

    for (auto &semaphore : this->image_acquired_semaphores)
    {
        vkDestroySemaphore(device.device, semaphore, nullptr);
        semaphore = VK_NULL_HANDLE;
    }

    for (auto &semaphore : this->can_present_semaphores)
    {
        vkDestroySemaphore(device.device, semaphore, nullptr);
        semaphore = VK_NULL_HANDLE;
    }

    vkDestroySwapchainKHR(device.device, this->swapchain, nullptr);
    this->swapchain = VK_NULL_HANDLE;
}

} // namespace vulkan
