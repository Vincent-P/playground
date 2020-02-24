#pragma once

#include <optional>
#include <vk_mem_alloc.h>

#include <vulkan/vulkan.hpp>
#include "types.hpp"

/***
 * VlkContext is a small abstraction over vulkan.
 * It is used by the HL API and the renderer to handle the recreation of the swapchain on resize
 ***/

namespace my_app
{
    class Window;

    namespace vulkan
    {

	struct SwapChain
	{
	    vk::UniqueSwapchainKHR handle;
	    std::vector<vk::Image> images;
	    std::vector<vk::ImageView> image_views;
	    vk::SurfaceFormatKHR format;
	    vk::PresentModeKHR present_mode;
	    vk::Extent2D extent;
	    u32 current_image;

	    vk::Image get_current_image() { return images[current_image]; }
	    vk::ImageView get_current_image_view() { return image_views[current_image]; }
	};


	struct FrameResource
	{
	    vk::UniqueFence fence;
	    vk::UniqueSemaphore image_available;
	    vk::UniqueSemaphore rendering_finished;
	    vk::UniqueFramebuffer framebuffer;
	    vk::UniqueCommandBuffer commandbuffer;
	};

	struct FrameResources
	{
	    std::vector<FrameResource> data;
	    usize current;

	    FrameResource& get_current() { return data[current]; }
	};

	struct Context
	{
	    vk::UniqueInstance instance;
	    std::optional<vk::DebugUtilsMessengerEXT> debug_messenger;
	    vk::UniqueSurfaceKHR surface;
	    vk::PhysicalDevice physical_device;
	    // gpu props?
	    // surface caps?
	    vk::UniqueDevice device;
	    VmaAllocator allocator;

	    u32 graphics_family_idx;
	    u32 present_family_idx;

	    vk::UniqueCommandPool command_pool;
	    SwapChain swapchain;
	    FrameResources frame_resources;
	    usize frame_count;

	    // query pool for timestamps

	    static Context create(const Window& window);
	    void create_swapchain();
	    void create_frame_resources(usize count = 2);
	    void destroy_swapchain();
	    void on_resize(int width, int height);
	    void destroy();
	};
    }
}
