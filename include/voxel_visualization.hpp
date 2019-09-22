#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "buffer.hpp"
#include "vulkan_context.hpp"
#include "model.hpp"

namespace my_app
{
    class Renderer;
    struct Camera;

    struct SceneUniform
    {
        glm::vec4 cam_pos;
        glm::vec4 cam_front;
        glm::vec4 cam_up;
    };

    class VoxelVisualization
    {
    public:
        explicit VoxelVisualization(Renderer&, uint32_t _subpass);
        ~VoxelVisualization();

        void init(vk::DescriptorSetLayout _voxels_texture_layout);
        void do_subpass(uint32_t resource_index, vk::CommandBuffer cmd);

        void update_uniform_buffer(uint32_t frame_idx, Camera& camera);

    private:
        void create_descriptors();
        void update_descriptors();
        void create_pipeline(vk::DescriptorSetLayout _voxels_texture_layout);

        Renderer& renderer;
        const VulkanContext& vulkan;
        uint32_t subpass;

        std::array<Buffer, NUM_VIRTUAL_FRAME> uniform_buffers;

        vk::UniqueDescriptorPool desc_pool;
        // Per frame descriptor
        MultipleDescriptorSet scenes;

        Pipeline graphics_pipeline;
    };
}
