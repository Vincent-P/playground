#pragma once

#include "tiny_gltf.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <optional>
#include <string>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "buffer.hpp"

namespace my_app
{
    constexpr float global_scale = .1f;

    static tinygltf::TinyGLTF loader;

    struct VulkanContext;

    struct Vertex
    {
        glm::vec3 pos;
        glm::vec3 normal;

        static std::array<vk::VertexInputBindingDescription, 1> getBindingDescriptions()
        {
            std::array<vk::VertexInputBindingDescription, 1> bindings;
            bindings[0].binding = 0;
            bindings[0].stride = sizeof(Vertex);
            bindings[0].inputRate = vk::VertexInputRate::eVertex;
            return bindings;
        }

        static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
        {
            std::array<vk::VertexInputAttributeDescription, 2> descs;
            descs[0].binding = 0;
            descs[0].location = 0;
            descs[0].format = vk::Format::eR32G32B32Sfloat;
            descs[0].offset = offsetof(Vertex, pos);

            descs[1].binding = 0;
            descs[1].location = 1;
            descs[1].format = vk::Format::eR32G32B32Sfloat;
            descs[1].offset = offsetof(Vertex, normal);

            return descs;
        }
    };

    struct Material
    {
        enum class AlphaMode
        {
            Opaque,
            Mask,
            Blend
        };

        enum class PbrWorkflow
        {
            MetallicRoughness,
            SpecularGlossiness
        };

        constexpr static float WorkflowFloat(PbrWorkflow workflow)
        {
            switch (workflow)
            {
                case PbrWorkflow::MetallicRoughness:
                    return 0;
                case PbrWorkflow::SpecularGlossiness:
                    return 1;
            }
        }

        AlphaMode alphaMode = AlphaMode::Opaque;
        float alphaCutoff = 1.0f;
        float metallicFactor = 1.0f;
        float roughnessFactor = 1.0f;
        glm::vec4 baseColorFactor = glm::vec4(1.0f);
        glm::vec4 emissiveFactor = glm::vec4(1.0f);

        // vkglTF::Texture *baseColorTexture;
        // vkglTF::Texture *metallicRoughnessTexture;
        // vkglTF::Texture *normalTexture;
        // vkglTF::Texture *occlusionTexture;
        // vkglTF::Texture *emissiveTexture;

        struct Extension
        {
            // vkglTF::Texture *specularGlossinessTexture;
            // vkglTF::Texture *diffuseTexture;
            glm::vec4 diffuseFactor = glm::vec4(1.0f);
            glm::vec3 specularFactor = glm::vec3(0.0f);
        } extension;

        PbrWorkflow workflow = PbrWorkflow::MetallicRoughness;

        vk::DescriptorSet desc_set;
    };

    struct PushConstBlockMaterial
    {
        glm::vec4 baseColorFactor;
        glm::vec4 emissiveFactor;
        glm::vec4 diffuseFactor;
        glm::vec4 specularFactor;
        float workflow;
        float hasColorTexture;
        float hasPhysicalDescriptorTexture;
        float hasNormalTexture;
        float hasOcclusionTexture;
        float hasEmissiveTexture;
        float metallicFactor;
        float roughnessFactor;
        float alphaMask;
        float alphaMaskCutoff;
    };

    struct Primitive
    {
        std::uint32_t first_vertex;
        std::uint32_t first_index;
        std::uint32_t index_count;
        Material& material;
    };

    struct Mesh
    {
        struct UniformBlock
        {
            glm::mat4 matrix;
        } uniform_block;

        Mesh(VulkanContext& ctx);
        void draw(vk::CommandBuffer& cmd, vk::PipelineLayout& pipeline_layout, vk::DescriptorSet& desc_set) const;

        std::vector<Primitive> primitives;
        Buffer uniform;
        vk::DescriptorSet uniform_desc;
    };

    struct Node
    {
        Node* parent = nullptr;
        std::vector<Node> children;

        Mesh* mesh = nullptr;

        glm::mat4 matrix;
        glm::vec3 translation{};
        glm::vec3 scale{1.0f};
        glm::quat rotation{};

        glm::mat4 localMatrix()
        {
            return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;
        }

        glm::mat4 getMatrix()
        {
            glm::mat4 m = localMatrix();
            auto p = parent;
            while (p)
            {
                m = p->localMatrix() * m;
                p = p->parent;
            }
            return m;
        }

        void update();
        void SetupNodeDescriptorSet(vk::DescriptorPool& desc_pool, vk::DescriptorSetLayout& desc_set_layout, vk::Device& device);

        void draw(vk::CommandBuffer& cmd, vk::PipelineLayout& pipeline_layout, vk::DescriptorSet& desc_set) const;
    };

    struct Model
    {
        Model(std::string path, VulkanContext& ctx);
        ~Model() = default;

        void LoadMaterials();
        void LoadMeshes(VulkanContext& ctx);
        Node LoadNode(size_t i);
        void LoadNodes();
        void Free();

        void draw(vk::CommandBuffer& cmd, vk::PipelineLayout& pipeline_layout, vk::DescriptorSet& desc_set) const;

        tinygltf::Model model;
        std::vector<Material> materials;
        std::vector<Mesh> meshes;
        std::vector<Node> scene_nodes;
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };
}    // namespace my_app
