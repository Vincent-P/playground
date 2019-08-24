#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include <vector>

#include "vulkan_context.hpp"
#include "model.hpp"
#include "image.hpp"
#include "buffer.hpp"

struct GLFWwindow;

namespace my_app
{
    constexpr int WIDTH = 1920;
    constexpr int HEIGHT = 1080;
    constexpr int NUM_VIRTUAL_FRAME = 2;

    struct Camera
    {
        glm::vec3 position = glm::vec3(0.0f, 0.0f, -2.0f);
        glm::vec3 front = glm::vec3(0.0f, 0.0f, 1.0f );
        glm::vec3 up = glm::vec3(0.0f, 1.0f,  0.0f);
        double yaw = 0.0;
        double pitch = 0.0;
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
        vk::Fence Fence;
        vk::UniqueSemaphore ImageAvailableSemaphore;
        vk::UniqueSemaphore RenderingFinishedSemaphore;
        vk::UniqueFramebuffer FrameBuffer;
        vk::UniqueCommandBuffer CommandBuffer;
    };

    struct SwapChain
    {
        vk::UniqueSwapchainKHR Handle;
        std::vector<vk::Image> Images;
        std::vector<vk::ImageView> ImageViews;
        vk::SurfaceFormatKHR Format;
        vk::PresentModeKHR PresentMode;
        vk::Extent2D Extent;
    };

    class Renderer
    {
    public:
        Renderer(GLFWwindow *window);
        Renderer(Renderer& other) = delete;
        ~Renderer();

        void CreateSwapchain();
        void DestroySwapchain();
        void RecreateSwapchain();

        void CreateFrameRessources();
        void CreateColorBuffer();
        void CreateDepthBuffer();
        void CreateUniformBuffer();
        void CreateDescriptors();
        void CreateRenderPass();
        void CreateIndexBuffer();
        void CreateVertexBuffer();
        void LoadShaders();

        void CreateGraphicsPipeline();

        void Resize(int width, int height);

        void UpdateUniformBuffer(float time, Camera& camera);
        void DrawFrame(double time, Camera& camera);
        void WaitIdle();

    private:

        VulkanContext vk_ctx_;

        Model Model_;
        SwapChain SwapChain_;
        std::vector<FrameRessource> FrameRessources_;

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

        vk::ShaderModule vert_module;
        vk::ShaderModule frag_module;

        vk::DescriptorPool desc_pool;

        vk::DescriptorSetLayout scene_desc_layout;
        vk::DescriptorSetLayout mat_desc_layout;
        vk::DescriptorSetLayout node_desc_layout;

        std::vector<vk::DescriptorSet> desc_sets;

        std::vector<vk::PipelineShaderStageCreateInfo> shader_stages;

        vk::Pipeline pipeline;
        vk::PipelineCache pipeline_cache;
        vk::PipelineLayout pipeline_layout;
        vk::RenderPass render_pass;
        std::vector<vk::Framebuffer> frame_buffers;

        // Settings
        vk::SampleCountFlagBits msaa_samples = vk::SampleCountFlagBits::e2;
    };
}
