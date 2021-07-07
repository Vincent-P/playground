#pragma once

#include "base/types.hpp"
#include "base/option.hpp"
#include "base/vector.hpp"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#if defined(ENABLE_RENDERDOC)
#include <renderdoc.h>
#endif

inline const char *vkres_to_str(VkResult code)
{
    switch (code)
    {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_EVENT_SET: return "VK_EVENT_SET";
    case VK_EVENT_RESET: return "VK_EVENT_RESET";
    case VK_INCOMPLETE: return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
    case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
    case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
    case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
    case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
    // case VK_ERROR_INCOMPATIBLE_VERSION_KHR: return "VK_ERROR_INCOMPATIBLE_VERSION_KHR";
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
    case VK_ERROR_NOT_PERMITTED_EXT: return "VK_ERROR_NOT_PERMITTED_EXT";
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
    case VK_THREAD_IDLE_KHR: return "VK_THREAD_IDLE_KHR";
    case VK_THREAD_DONE_KHR: return "VK_THREAD_DONE_KHR";
    case VK_OPERATION_DEFERRED_KHR: return "VK_OPERATION_DEFERRED_KHR";
    case VK_OPERATION_NOT_DEFERRED_KHR: return "VK_OPERATION_NOT_DEFERRED_KHR";
    case VK_PIPELINE_COMPILE_REQUIRED_EXT: return "VK_PIPELINE_COMPILE_REQUIRED_EXT";
    default: break;
    }
    return "Unkown VkResult";
}

#define VK_CHECK(x)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        VkResult err = x;                                                                                              \
        if (err)                                                                                                       \
        {                                                                                                              \
            const char *err_msg = vkres_to_str(err);                                                                   \
            fmt::print(stderr, "Vulkan function returned {}\n", err_msg);                                              \
            throw std::runtime_error(err_msg);                                                                         \
        }                                                                                                              \
    } while (0)

bool operator==(const VkPipelineShaderStageCreateInfo &a, const VkPipelineShaderStageCreateInfo &b);
bool operator==(const VkDescriptorBufferInfo &a, const VkDescriptorBufferInfo &b);
bool operator==(const VkDescriptorImageInfo &a, const VkDescriptorImageInfo &b);
bool operator==(const VkExtent3D &a, const VkExtent3D &b);
bool operator==(const VkImageSubresourceRange &a, const VkImageSubresourceRange &b);
bool operator==(const VkImageCreateInfo &a, const VkImageCreateInfo &b);
bool operator==(const VkComputePipelineCreateInfo &a, const VkComputePipelineCreateInfo &b);
bool operator==(const VkFramebufferCreateInfo &a, const VkFramebufferCreateInfo &b);

namespace my_app
{
namespace platform {struct Window;}

namespace vulkan
{

constexpr inline auto ENABLE_VALIDATION_LAYERS = true;
constexpr inline auto FRAMES_IN_FLIGHT         = 2;
constexpr inline u32  MAX_TIMESTAMP_PER_FRAME  = 512;

struct SwapChain
{
    VkSwapchainKHR handle;
    VkSurfaceFormatKHR format;
    VkPresentModeKHR present_mode;
    VkExtent2D extent;
    u32 current_image = 0;
    u32 images_count = 0;

    Vec<VkImage> images;
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
    Vec<FrameResource> data;
    usize current;

    FrameResource &get_current() { return data[current]; }
};

struct Context
{
    VkInstance instance;

    Option<VkDebugUtilsMessengerEXT> debug_messenger;

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

    static void create(Context &ctx, const platform::Window &window);
    void create_swapchain();
    void create_frame_resources(usize count = 1);
    void destroy_swapchain();
    void on_resize();
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
