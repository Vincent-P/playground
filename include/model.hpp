#pragma once

#include "tiny_gltf.h"
#include <string>
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <vk_mem_alloc.h>

namespace my_app
{
    static tinygltf::TinyGLTF loader;

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
        void draw(vk::CommandBuffer& cmd, vk::PipelineLayout& pipeline_layout) const;

        std::vector<Primitive> primitives;
    };

    struct Model
    {
        Model(std::string);
        ~Model() = default;

        void LoadMaterials();
        void LoadMeshesBuffers();

        void draw(vk::CommandBuffer& cmd, vk::PipelineLayout& pipeline_layout) const;

        tinygltf::Model model;
        std::vector<Material> materials;
        std::vector<Mesh> meshes;
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };
}    // namespace my_app
