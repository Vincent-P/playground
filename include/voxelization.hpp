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

    struct VoxelizationOptions
    {
        glm::vec3 center;
        float size;
        uint32_t res;
    };

    class VoxelizationSubpass
    {
    public:
        explicit VoxelizationSubpass(Renderer&, uint32_t _subpass);
        ~VoxelizationSubpass();

        void init(const std::string& model_path);
        void before_subpass(uint32_t resource_index, vk::CommandBuffer cmd);
        void do_subpass(uint32_t resource_index, vk::CommandBuffer cmd);
        void update_uniform_buffer(uint32_t frame_idx);

        vk::DescriptorSetLayout get_voxels_texture_layout() const
        {
            return voxels.layout.get();
        }

    private:
        void create_empty();
        void create_descriptors();
        void update_descriptors();
        void create_pipeline();

        void update_meshes_uniform(Node& node);

        Renderer& renderer;
        const VulkanContext& vulkan;
        uint32_t subpass;

        Model model;
        Image empty_image;
        vk::DescriptorImageInfo empty_info;
        Buffer index_buffer;
        Buffer vertex_buffer;


        vk::UniqueDescriptorPool desc_pool;

        // Global descriptor
        Image voxels_texture;
        DescriptorSet voxels;

        // Per frame descriptor
        std::array<Buffer, NUM_VIRTUAL_FRAME> debug_options;
        MultipleDescriptorSet debug_voxel;

        // Per mesh descriptor
        std::vector<Buffer> mesh_buffers;
        MultipleDescriptorSet transforms;

        // Per primitive descriptor
        MultipleDescriptorSet materials;

        Pipeline graphics_pipeline;
    };
}
