#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "buffer.hpp"
#include "gui.hpp"
#include "image.hpp"
#include "model.hpp"
#include "vulkan_context.hpp"

struct GLFWwindow;

namespace my_app
{
    constexpr inline int WIDTH = 1920;
    constexpr inline int HEIGHT = 1080;
    constexpr inline int NUM_VIRTUAL_FRAME = 2;
    constexpr inline vk::SampleCountFlagBits MSAA_SAMPLES = vk::SampleCountFlagBits::e2;
    constexpr inline unsigned VOXEL_GRID_SIZE = 256;

    struct Camera
    {
        glm::vec3 position = glm::vec3(0.0f, 0.0f, -2.0f);
        glm::vec3 front = glm::vec3(0.0f, 0.0f, 1.0f);
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        float yaw = 0.0f;
        float pitch = 0.0f;
    };

    struct SceneUniform
    {
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 clip;
        glm::vec4 cam_pos;
        glm::vec4 light_dir;
        float debug_view_input;
        float debug_view_equation;
        float ambient;
        float dummy;
    };

    struct FrameRessource
    {
        vk::UniqueFence fence;
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

    struct Voxel
    {
        glm::vec4 color;
        glm::vec4 normal;


        static std::array<vk::VertexInputBindingDescription, 1> get_binding_description()
        {
            std::array<vk::VertexInputBindingDescription, 1> bindings;
            bindings[0].binding = 0;
            bindings[0].stride = sizeof(Voxel);
            bindings[0].inputRate = vk::VertexInputRate::eVertex;
            return bindings;
        }

        static std::array<vk::VertexInputAttributeDescription, 2> get_attribute_description()
        {
            std::array<vk::VertexInputAttributeDescription, 2> descs;
            descs[0].binding = 0;
            descs[0].location = 0;
            descs[0].format = vk::Format::eR32G32B32A32Sfloat;
            descs[0].offset = offsetof(Voxel, color);

            descs[1].binding = 0;
            descs[1].location = 1;
            descs[1].format = vk::Format::eR32G32B32A32Sfloat;
            descs[1].offset = offsetof(Voxel, normal);
            return descs;
        }
    };

    class Renderer
    {
        public:
        Renderer(GLFWwindow* window, const std::string &model_path);
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
        void create_voxels_buffer();

        void create_graphics_pipeline();
        void create_debug_graphics_pipeline();

        void resize(int width, int height);

        void update_uniform_buffer(FrameRessource* frame_ressource, Camera& camera);
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
        Buffer voxels_buffer;

        vk::UniqueShaderModule vert_module;
        vk::UniqueShaderModule frag_module;

        vk::UniqueDescriptorPool desc_pool;

        // Descriptor layout of per scene
        vk::UniqueDescriptorSetLayout scene_desc_layout;
        // Descriptor layout of per material
        vk::UniqueDescriptorSetLayout mat_desc_layout;
        // Descriptor layout of per object
        vk::UniqueDescriptorSetLayout node_desc_layout;
        vk::UniqueDescriptorSetLayout voxels_desc_layout;

        std::vector<vk::UniqueDescriptorSet> desc_sets;
        vk::UniqueDescriptorSet voxels_desc_set;

        vk::UniquePipeline pipeline_debug_voxels;
        vk::UniquePipelineCache pipeline_cache_debug_voxels;
        vk::UniquePipelineLayout pipeline_layout_debug_voxels;

        vk::UniquePipeline pipeline;
        vk::UniquePipelineCache pipeline_cache;
        vk::UniquePipelineLayout pipeline_layout;
        vk::UniqueRenderPass render_pass;
    };
}    // namespace my_app
