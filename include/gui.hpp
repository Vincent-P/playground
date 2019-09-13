#pragma once
#include <vector>
#include <vulkan/vulkan.hpp>

#include "buffer.hpp"
#include "image.hpp"

namespace my_app
{
    struct VulkanContext;
    class Renderer;
    class TimerData;

    struct GUIFrameRessource
    {
        Buffer vertex_buffer;
        Buffer index_buffer;
    };

    class GUI
    {
        public:
        explicit GUI(Renderer&, uint32_t _subpass);
        ~GUI();

        void init();
        void do_subpass(uint32_t resource_index, vk::CommandBuffer cmd);
        void start_frame(const TimerData& timer) const;

        private:
        void draw_frame_data(uint32_t resource_index, vk::CommandBuffer cmd);
        void create_texture();
        void create_descriptors();
        void create_render_pass();
        void create_pipeline_layout();
        void create_graphics_pipeline();

        Renderer& renderer;
        const VulkanContext& vulkan;
        uint32_t subpass;

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
