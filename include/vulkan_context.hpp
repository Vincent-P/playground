#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include <optional>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#pragma clang diagnostic pop

struct GLFWwindow;

namespace my_app
{
    constexpr bool ENABLE_VALIDATION_LAYERS = true;
    constexpr std::array<const char*, 1> VALIDATION_LAYERS = {
        "VK_LAYER_LUNARG_standard_validation",
    };

    constexpr std::array<const char*, 1> DEVICE_EXTENSIONS = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };


    struct VulkanContext
    {
        VulkanContext(GLFWwindow* window);
        VulkanContext(VulkanContext& other) = delete;
        ~VulkanContext();

        // Constructor
        static vk::UniqueInstance create_instance();
        std::optional<vk::DebugUtilsMessengerEXT> setup_messenger();
        vk::UniqueSurfaceKHR create_surface(GLFWwindow* window);
        vk::PhysicalDevice pick_physical_device();
        vk::UniqueDevice create_logical_device();
        VmaAllocator init_allocator();

        // Utility
        vk::Queue get_graphics_queue() const;
        vk::Queue get_present_queue() const;

        void transition_layout(vk::PipelineStageFlagBits src, vk::PipelineStageFlagBits dst, vk::ImageMemoryBarrier barrier) const;
        vk::UniqueShaderModule create_shader_module(std::vector<char> code) const;

        size_t graphics_family_idx;
        size_t present_family_idx;

        vk::UniqueInstance instance;
        vk::DispatchLoaderDynamic dldi;
        std::optional<vk::DebugUtilsMessengerEXT> debug_messenger;
        vk::UniqueSurfaceKHR surface;
        vk::PhysicalDevice physical_device;
        vk::UniqueDevice device;
        VmaAllocator allocator;
        vk::CommandPool command_pool;
    };

}    // namespace my_app
