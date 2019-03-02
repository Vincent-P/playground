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
#include "image.hpp"

namespace my_app
{
    constexpr float global_scale = 5.;

    static tinygltf::TinyGLTF loader;

    struct VulkanContext;

    constexpr vk::SamplerAddressMode GetVkWrapMode(int32_t wrapMode)
    {
        switch (wrapMode)
        {
            case TINYGLTF_TEXTURE_WRAP_REPEAT:
                return vk::SamplerAddressMode::eRepeat;
            case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                return vk::SamplerAddressMode::eClampToEdge;
            case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
                return vk::SamplerAddressMode::eMirroredRepeat;
        }
    }

    constexpr vk::Filter GetVkFilterMode(int32_t filterMode)
    {
        switch (filterMode)
        {
            case TINYGLTF_TEXTURE_FILTER_NEAREST:
                return vk::Filter::eNearest;
            case TINYGLTF_TEXTURE_FILTER_LINEAR:
                return vk::Filter::eLinear;
            case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
                return vk::Filter::eNearest;
            case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
                return vk::Filter::eNearest;
            case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
                return vk::Filter::eLinear;
            case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
                return vk::Filter::eLinear;
        }
    }

    struct TextureSampler
    {
        vk::Filter magFilter = vk::Filter::eNearest;
        vk::Filter minFilter = vk::Filter::eLinear;
        vk::SamplerAddressMode addressModeU = vk::SamplerAddressMode::eRepeat;
        vk::SamplerAddressMode addressModeV = vk::SamplerAddressMode::eRepeat;
        vk::SamplerAddressMode addressModeW = vk::SamplerAddressMode::eRepeat;
    };

    struct Texture
    {
        Image image;
        vk::DescriptorImageInfo desc_info;
        uint32_t width, height;
        uint32_t mip_levels;
        uint32_t layerCount;

        Texture(VulkanContext& ctx, tinygltf::Image& gltf_image, TextureSampler& sampler);
    };

    struct Vertex
    {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 uv0;
        glm::vec2 uv1;

        static std::array<vk::VertexInputBindingDescription, 1> getBindingDescriptions()
        {
            std::array<vk::VertexInputBindingDescription, 1> bindings;
            bindings[0].binding = 0;
            bindings[0].stride = sizeof(Vertex);
            bindings[0].inputRate = vk::VertexInputRate::eVertex;
            return bindings;
        }

        static std::array<vk::VertexInputAttributeDescription, 4> getAttributeDescriptions()
        {
            std::array<vk::VertexInputAttributeDescription, 4> descs;
            descs[0].binding = 0;
            descs[0].location = 0;
            descs[0].format = vk::Format::eR32G32B32Sfloat;
            descs[0].offset = offsetof(Vertex, pos);

            descs[1].binding = 0;
            descs[1].location = 1;
            descs[1].format = vk::Format::eR32G32B32Sfloat;
            descs[1].offset = offsetof(Vertex, normal);

            descs[2].binding = 0;
            descs[2].location = 2;
            descs[2].format = vk::Format::eR32G32Sfloat;
            descs[2].offset = offsetof(Vertex, uv0);

            descs[3].binding = 0;
            descs[3].location = 3;
            descs[3].format = vk::Format::eR32G32Sfloat;
            descs[3].offset = offsetof(Vertex, uv1);

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

        Texture* baseColorTexture;
        Texture* metallicRoughnessTexture;
        Texture* normalTexture;
        Texture* occlusionTexture;
        Texture* emissiveTexture;

        struct TexCoordSets
        {
            uint8_t baseColor = 0;
            uint8_t metallicRoughness = 0;
            uint8_t specularGlossiness = 0;
            uint8_t normal = 0;
            uint8_t occlusion = 0;
            uint8_t emissive = 0;
        } texCoordSets;

        struct Extension
        {
            Texture* specularGlossinessTexture;
            Texture* diffuseTexture;
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
        int colorTextureSet;
        int PhysicalDescriptorTextureSet;
        int normalTextureSet;
        int occlusionTextureSet;
        int emissiveTextureSet;
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

        glm::mat4 matrix{1.0f};
        glm::vec3 translation;
        glm::vec3 scale{1.0f};
        glm::mat4 rotation{1.0f};

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

        void LoadTextures();
        void LoadSamplers();
        void LoadMaterials();
        void LoadMeshes();
        Node LoadNode(size_t i);
        void LoadNodes();
        void Free();

        void draw(vk::CommandBuffer& cmd, vk::PipelineLayout& pipeline_layout, vk::DescriptorSet& desc_set) const;

        VulkanContext &ctx;
        tinygltf::Model model;
        std::vector<TextureSampler> text_samplers;
        std::vector<Texture> textures;
        std::vector<Material> materials;
        std::vector<Mesh> meshes;
        std::vector<Node> scene_nodes;
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };
}    // namespace my_app
