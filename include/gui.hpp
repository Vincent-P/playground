#pragma once
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include <vector>
#include <vulkan/vulkan.hpp>
#pragma clang diagnostic pop

#include "buffer.hpp"
#include "image.hpp"

namespace my_app
{
    class Renderer;
    class TimerData;
    namespace tools
    {
        struct MouseState;
    }

    struct GUIFrameRessource
    {
        Buffer vertex_buffer;
        Buffer index_buffer;
    };

    class GUI
    {
        public:
        GUI(const Renderer& renderer);
        ~GUI();

        void init();
        void start_frame(const TimerData& timer, const tools::MouseState& mouse);
        void draw(uint32_t resource_index, vk::UniqueCommandBuffer& command_buffer, vk::UniqueRenderPass const& render_pass, vk::UniqueFramebuffer const& framebuffer);

        private:
        void draw_frame_data(vk::UniqueCommandBuffer& command_buffer, uint32_t resource_index);
        void create_texture();
        void create_descriptors();
        void create_render_pass();
        void create_pipeline_layout();
        void create_graphics_pipeline();

        const Renderer& parent;

        vk::UniqueRenderPass render_pass;

        vk::UniquePipeline pipeline;
        vk::UniquePipelineCache pipeline_cache;
        vk::UniquePipelineLayout pipeline_layout;

        vk::UniqueDescriptorPool desc_pool;
        vk::UniqueDescriptorSetLayout desc_layout;
        vk::UniqueDescriptorSet desc_set;

        std::vector<GUIFrameRessource> resources;

        Image texture;
        vk::DescriptorImageInfo texture_desc_info;
    };
}    // namespace my_app
