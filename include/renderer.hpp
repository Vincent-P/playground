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
        void create_uniform_buffer();
        void create_descriptors();
        void create_render_pass();
        void create_index_buffer();
        void create_vertex_buffer();

        void create_graphics_pipeline();

        void resize(int width, int height);

        void update_uniform_buffer(float time, Camera& camera);
        void draw_frame(double time, Camera& camera);
        void wait_idle();

        private:
        VulkanContext vulkan;

        Model model;
        SwapChain swapchain;
        std::vector<FrameRessource> frame_ressources;

        // Attachments
        Image depth_image;
        vk::ImageView depth_image_view;
        vk::Format depth_format;

        Image color_image;
        vk::ImageView color_image_view;

        Image empty_image;
        vk::DescriptorImageInfo empty_info;

        Buffer uniform_buffer;
        Buffer index_buffer;
        Buffer vertex_buffer;

        vk::UniqueShaderModule vert_module;
        vk::UniqueShaderModule frag_module;

        vk::DescriptorPool desc_pool;

        vk::DescriptorSetLayout scene_desc_layout;
        vk::DescriptorSetLayout mat_desc_layout;
        vk::DescriptorSetLayout node_desc_layout;

        std::vector<vk::DescriptorSet> desc_sets;

        vk::Pipeline pipeline;
        vk::PipelineCache pipeline_cache;
        vk::PipelineLayout pipeline_layout;
        vk::RenderPass render_pass;
    };
}    // namespace my_app