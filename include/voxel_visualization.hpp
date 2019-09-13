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
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 clip;
        glm::vec4 cam_pos;
        glm::vec4 light_dir;
        float debug_view_input;
        float debug_view_equation;
        float ambient;
        float cube_scale;
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

    class VoxelVisualization
    {
    public:
        explicit VoxelVisualization(Renderer&, uint32_t _subpass);
        ~VoxelVisualization();

        void init();
        void do_subpass(uint32_t resource_index, vk::CommandBuffer cmd, const Buffer& voxels_buffer);

        void update_uniform_buffer(uint32_t frame_idx, Camera& camera);

    private:
        void create_descriptors();
        void update_descriptors();
        void create_pipeline();

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
