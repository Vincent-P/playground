#pragma once

#include <optional>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <thsvs/thsvs_simpler_vulkan_synchronization.h>

#include "buffer.hpp"
#include "image.hpp"

struct GLFWwindow;

namespace my_app
{
    constexpr inline int WIDTH = 1920;
    constexpr inline int HEIGHT = 1080;
    constexpr inline int NUM_VIRTUAL_FRAME = 2;
    constexpr inline vk::SampleCountFlagBits MSAA_SAMPLES = vk::SampleCountFlagBits::e2;
    constexpr inline unsigned VOXEL_GRID_SIZE = 256;
    constexpr bool ENABLE_VALIDATION_LAYERS = true;

    constexpr std::array<const char*, 1> VALIDATION_LAYERS = {
        "VK_LAYER_LUNARG_standard_validation",
    };

    constexpr std::array<const char*, 1> DEVICE_EXTENSIONS = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    struct DescriptorSet
    {
        vk::UniqueDescriptorSetLayout layout;
        vk::UniqueDescriptorSet descriptor;
    };

    struct MultipleDescriptorSet
    {
        vk::UniqueDescriptorSetLayout layout;
        std::vector<vk::UniqueDescriptorSet> descriptors;
    };

    struct Pipeline
    {
        vk::UniquePipeline handle;
        vk::UniquePipelineCache cache;
        vk::UniquePipelineLayout layout;
    };

    struct VulkanContext
    {
        explicit VulkanContext(GLFWwindow* window);
        VulkanContext(VulkanContext& other) = delete;
        ~VulkanContext();

        // Constructor
        static vk::UniqueInstance create_instance();
        std::optional<vk::DebugUtilsMessengerEXT> setup_messenger();
        vk::UniqueSurfaceKHR create_surface(GLFWwindow* window);
        vk::PhysicalDevice pick_physical_device();
        vk::UniqueDevice create_logical_device();
        VmaAllocator init_allocator() const;

        // Utility
        vk::Queue get_graphics_queue() const;
        vk::Queue get_present_queue() const;

        // Ressources
        vk::UniqueShaderModule create_shader_module(std::vector<char> code) const;
        vk::UniqueDescriptorSetLayout create_descriptor_layout(std::vector<vk::DescriptorSetLayoutBinding> bindings) const;

        void transition_layout_cmd(vk::CommandBuffer cmd, vk::Image image, ThsvsAccessType prev_access, ThsvsAccessType next_access, vk::ImageSubresourceRange subresource_range) const;
        void transition_layout(vk::Image image, ThsvsAccessType prev_access, ThsvsAccessType next_access, vk::ImageSubresourceRange subresource_range) const;

        // Command buffers operations
        void submit_and_wait_cmd(vk::CommandBuffer) const;

        struct CopyDataToImageParams
        {
            CopyDataToImageParams(const Image& _target_image, const vk::ImageSubresourceRange& _range)
                : target_image(_target_image)
                , subresource_range(_range)
            {}

            const Image& target_image;
            void const* data;
            size_t data_size;
            uint32_t width;
            uint32_t height;
            const vk::ImageSubresourceRange &subresource_range;
            ThsvsAccessType current_image_access;
            ThsvsAccessType next_image_access;
        };

        Buffer copy_data_to_image_cmd(vk::CommandBuffer cmd, CopyDataToImageParams params) const;
        void copy_data_to_image(CopyDataToImageParams params) const;

        struct CopyDataToBufferParams
        {
            CopyDataToBufferParams(const Buffer& _buffer)
                : buffer(_buffer)
            {}
            const Buffer &buffer;
            void const* data;
            size_t data_size;
            vk::AccessFlags current_buffer_access;
            vk::PipelineStageFlags generating_stages;
            vk::AccessFlags new_buffer_access;
            vk::PipelineStageFlags consuming_stages;
        };
        Buffer copy_data_to_buffer_cmd(vk::CommandBuffer cmd, CopyDataToBufferParams params) const;
        void copy_data_to_buffer(CopyDataToBufferParams params) const;

        void clear_buffer_cmd(vk::CommandBuffer cmd, const Buffer& buffer, uint32_t data) const;
        void clear_buffer(const Buffer& buffer, uint32_t data) const;

        uint32_t graphics_family_idx;
        uint32_t present_family_idx;

        vk::UniqueInstance instance;
        vk::DispatchLoaderDynamic dldi;
        std::optional<vk::DebugUtilsMessengerEXT> debug_messenger;
        vk::UniqueSurfaceKHR surface;
        vk::PhysicalDevice physical_device;
        vk::UniqueDevice device;
        VmaAllocator allocator;
        vk::UniqueCommandPool command_pool;
        vk::UniqueCommandBuffer texture_command_buffer;
    };

}    // namespace my_app
