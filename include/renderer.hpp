#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>
#pragma clang diagnostic pop

#include "buffer.hpp"
#include "image.hpp"
#include "model.hpp"
#include "gui.hpp"
#include "vulkan_context.hpp"

struct GLFWwindow;

namespace my_app
{
    constexpr int WIDTH = 1920;
    constexpr int HEIGHT = 1080;
    constexpr int NUM_VIRTUAL_FRAME = 2;
    constexpr vk::SampleCountFlagBits MSAA_SAMPLES = vk::SampleCountFlagBits::e2;

    struct Camera
    {
        glm::vec3 position = glm::vec3(0.0f, 0.0f, -2.0f);
        glm::vec3 front = glm::vec3(0.0f, 0.0f, 1.0f);
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        float yaw = 0.0f;
        float pitch = 0.0f;
    };

    struct MVP
    {
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
        alignas(16) glm::mat4 clip;
        alignas(16) glm::vec3 cam_pos;
    };

    struct FrameRessource
    {
        vk::Fence fence;
        vk::UniqueSemaphore image_available;
        vk::UniqueSemaphore rendering_finished;
        vk::UniqueFramebuffer framebuffer;
        vk::UniqueCommandBuffer commandbuffer;
        Buffer uniform_buffer;
    };

    struct SwapChain
    {
        vk::UniqueSwapchainKHR handle;
        std::vector<vk::Image> images;
        std::vector<vk::ImageView> image_views;
        vk::SurfaceFormatKHR format;
        vk::PresentModeKHR present_mode;
        vk::Extent2D extent;
    };

    class Renderer
    {
        public:
        Renderer(GLFWwindow* window);
        Renderer(Renderer& other) = delete;
        ~Renderer();

        void create_swapchain();
        void destroy_swapchain();
        void recreate_swapchain();

        void create_frame_ressources();
        void create_color_buffer();
        void create_depth_buffer();
        void create_descriptors();
        void create_render_pass();
        void create_index_buffer();
        void create_vertex_buffer();

        void create_graphics_pipeline();

        void resize(int width, int height);

        void update_uniform_buffer(FrameRessource* frame_ressource, Camera& camera);
        void draw_frame(Camera& camera, const TimerData& timer, tools::MouseState mouse);
        void wait_idle();

        const VulkanContext& get_vulkan() const
        {
            return vulkan;
        }

        const SwapChain& get_swapchain() const
        {
            return swapchain;
        }

        vk::Format get_depth_format() const
        {
            return depth_format;
        }

        private:
        VulkanContext vulkan;

        Model model;
        GUI gui;
        SwapChain swapchain;
        std::vector<FrameRessource> frame_resources;

        // Attachments
        Image depth_image;
        vk::ImageView depth_image_view;
        vk::Format depth_format;

        Image color_image;
        vk::ImageView color_image_view;

        Image empty_image;
        vk::DescriptorImageInfo empty_info;

        Buffer index_buffer;
        Buffer vertex_buffer;

        vk::UniqueShaderModule vert_module;
        vk::UniqueShaderModule frag_module;

        vk::UniqueDescriptorPool desc_pool;
        vk::UniqueDescriptorSetLayout scene_desc_layout;
        vk::UniqueDescriptorSetLayout mat_desc_layout;
        vk::UniqueDescriptorSetLayout node_desc_layout;
        std::vector<vk::UniqueDescriptorSet> desc_sets;

        vk::UniquePipeline pipeline;
        vk::UniquePipelineCache pipeline_cache;
        vk::UniquePipelineLayout pipeline_layout;
        vk::UniqueRenderPass render_pass;
    };
}    // namespace my_app
