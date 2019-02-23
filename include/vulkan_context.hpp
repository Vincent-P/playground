#pragma once
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>
#include <optional>

struct GLFWwindow;

namespace my_app
{
    constexpr bool enable_validation_layers = true;
    constexpr std::array<const char*, 1> g_validation_layers =
    {
        "VK_LAYER_LUNARG_standard_validation"
    };

    constexpr std::array<const char*, 1> g_device_extensions =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    class VulkanContext
    {
    public:
        VulkanContext(GLFWwindow *window);
        VulkanContext(VulkanContext& other) = delete;
        ~VulkanContext();

        vk::UniqueInstance CreateInstance();
        std::optional<vk::DebugUtilsMessengerEXT> SetupDebugCallback();
        void CreateSurface(GLFWwindow *window);
        void CreateLogicalDevice();
        void InitAllocator();

        vk::UniqueInstance instance;
        vk::DispatchLoaderDynamic dldi;
        std::optional<vk::DebugUtilsMessengerEXT> debug_messenger;

        VmaAllocator allocator;
        vk::UniqueSurfaceKHR surface;

        vk::PhysicalDevice physical_device;
        vk::UniqueDevice device;
        vk::DispatchLoaderDynamic dldid;

        size_t graphics_family_idx;
        size_t present_family_idx;
    };

}
