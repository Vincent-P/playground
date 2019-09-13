#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "buffer.hpp"
#include "gui.hpp"
#include "image.hpp"
#include "model.hpp"
#include "vulkan_context.hpp"
#include "voxelization.hpp"
#include "voxel_visualization.hpp"

struct GLFWwindow;

namespace my_app
{
    struct Camera
    {
        glm::vec3 position = glm::vec3(0.0f, 0.0f, -2.0f);
        glm::vec3 front = glm::vec3(0.0f, 0.0f, 1.0f);
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        float yaw = 0.0f;
        float pitch = 0.0f;
    };

    struct FrameRessource
    {
        vk::UniqueFence fence;
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
        explicit Renderer(GLFWwindow* window, const std::string& model_path);
        Renderer(Renderer& other) = delete;
        ~Renderer();

        void create_swapchain();
        void destroy_swapchain();
        void recreate_swapchain();

        void create_frame_ressources();
        void create_color_buffer();
        void create_depth_buffer();

        void create_render_pass();

        void resize(int width, int height);

        void draw_frame(Camera& camera, const TimerData& timer);
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

        const vk::RenderPass get_render_pass() const
        {
            return render_pass.get();
        }

        private:
        VulkanContext vulkan;

        // subpasses
        VoxelizationSubpass voxelization;
        VoxelVisualization voxel_visualization;
        GUI gui;


        SwapChain swapchain;
        std::vector<FrameRessource> frame_resources;

        // Attachments
        Image depth_image;
        vk::ImageView depth_image_view;
        vk::Format depth_format;

        Image color_image;
        vk::ImageView color_image_view;

        vk::UniqueRenderPass render_pass;
    };
}    // namespace my_app
