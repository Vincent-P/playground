#pragma once

#include <optional>
#include <vk_mem_alloc.h>

#include "types.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define VK_CHECK(x)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        VkResult err = x;                                                                                              \
        if (err)                                                                                                       \
        {                                                                                                              \
            std::string error("Vulkan error");                                                                         \
            error = std::to_string(err) + std::string(".");                                                            \
            std::cerr << error << std::endl;                                                                           \
            throw std::runtime_error(error);                                                                           \
        }                                                                                                              \
    } while (0)


bool operator==(const VkPipelineShaderStageCreateInfo &a, const VkPipelineShaderStageCreateInfo &b);
bool operator==(const VkDescriptorBufferInfo &a, const VkDescriptorBufferInfo &b);
bool operator==(const VkDescriptorImageInfo &a, const VkDescriptorImageInfo &b);
bool operator==(const VkExtent3D &a, const VkExtent3D &b);
bool operator==(const VkImageSubresourceRange &a, const VkImageSubresourceRange &b);
bool operator==(const VkImageCreateInfo &a, const VkImageCreateInfo &b);
bool operator==(const VkComputePipelineCreateInfo &a, const VkComputePipelineCreateInfo &b);

namespace my_app
{

class Window;

namespace vulkan
{

constexpr inline auto ENABLE_VALIDATION_LAYERS = true;
constexpr inline auto FRAMES_IN_FLIGHT         = 1;
constexpr inline u32  MAX_TIMESTAMP_PER_FRAME  = 128;

struct SwapChain
{
    VkSwapchainKHR handle;
    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;
    VkSurfaceFormatKHR format;
    VkPresentModeKHR present_mode;
    VkExtent2D extent;
    u32 current_image;

    VkImage get_current_image() { return images[current_image]; }
    VkImageView get_current_image_view() { return image_views[current_image]; }
};

struct FrameResource
{
    VkFence fence;
    VkSemaphore image_available;
    VkSemaphore rendering_finished;

    VkCommandPool command_pool;
    VkCommandBuffer command_buffer; // main command buffer
};

struct FrameResources
{
    std::vector<FrameResource> data;
    usize current;

    FrameResource &get_current() { return data[current]; }
};

struct Context
{
    VkInstance instance;

    std::optional<VkDebugUtilsMessengerEXT> debug_messenger;

    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    VkPhysicalDeviceProperties physical_props;

    VkPhysicalDeviceVulkan12Features vulkan12_features;
    VkPhysicalDeviceFeatures2 physical_device_features;

    // gpu props?
    // surface caps?
    VkDevice device;
    VmaAllocator allocator;

    u32 graphics_family_idx;
    u32 present_family_idx;

    VkDescriptorPool descriptor_pool;

    SwapChain swapchain;
    FrameResources frame_resources;
    usize frame_count{0};
    usize descriptor_sets_count{0};

    // query pool for timestamps
    VkQueryPool timestamp_pool;

    static Context create(const Window &window);
    void create_swapchain();
    void create_frame_resources(usize count = 1);
    void destroy_swapchain();
    void on_resize(int width, int height);
    void destroy();


    // Instance functions
#define X(name) PFN_##name name
    X(vkCreateDebugUtilsMessengerEXT);
    X(vkDestroyDebugUtilsMessengerEXT);
    X(vkCmdBeginDebugUtilsLabelEXT);
    X(vkCmdEndDebugUtilsLabelEXT);
    X(vkSetDebugUtilsObjectNameEXT);
#undef X
};

} // namespace vulkan

} // namespace my_app
