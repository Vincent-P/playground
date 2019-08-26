#pragma once
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <optional>
#include <string>
#include <tiny_gltf.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#pragma clang diagnostic pop

#include "buffer.hpp"
#include "image.hpp"

namespace my_app
{
    struct VulkanContext;

    constexpr float global_scale = 5.;
    inline tinygltf::TinyGLTF loader;

    constexpr vk::SamplerAddressMode get_vk_wrap_mode(int32_t wrap_mode)
    {
        switch (wrap_mode)
        {
            case TINYGLTF_TEXTURE_WRAP_REPEAT:
                return vk::SamplerAddressMode::eRepeat;
            case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                return vk::SamplerAddressMode::eClampToEdge;
            case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
                return vk::SamplerAddressMode::eMirroredRepeat;
        }
        return vk::SamplerAddressMode::eRepeat;
    }

    constexpr vk::Filter get_vk_filter_mode(int32_t filter_mode)
    {
        switch (filter_mode)
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
        return vk::Filter::eLinear;
    }

    struct TextureSampler
    {
        vk::Filter mag_filter = vk::Filter::eNearest;
        vk::Filter min_filter = vk::Filter::eLinear;
        vk::SamplerAddressMode address_mode_u = vk::SamplerAddressMode::eRepeat;
        vk::SamplerAddressMode address_mode_v = vk::SamplerAddressMode::eRepeat;
        vk::SamplerAddressMode address_mode_w = vk::SamplerAddressMode::eRepeat;
    };

    struct Texture
    {
        Image image;
        vk::DescriptorImageInfo desc_info;
        uint32_t width, height;
        uint32_t mip_levels;
        uint32_t layer_count;

        Texture(VulkanContext& ctx, tinygltf::Image& gltf_image, TextureSampler& sampler);
    };

    struct Vertex
    {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 uv0;
        glm::vec2 uv1;

        static std::array<vk::VertexInputBindingDescription, 1> get_binding_description()
        {
            std::array<vk::VertexInputBindingDescription, 1> bindings;
            bindings[0].binding = 0;
            bindings[0].stride = sizeof(Vertex);
            bindings[0].inputRate = vk::VertexInputRate::eVertex;
            return bindings;
        }

        static std::array<vk::VertexInputAttributeDescription, 4> get_attribute_description()
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

        constexpr static float workflow_float(PbrWorkflow workflow)
        {
            switch (workflow)
            {
                case PbrWorkflow::MetallicRoughness:
                    return 0;
                case PbrWorkflow::SpecularGlossiness:
                    return 1;
            }
        }

        AlphaMode alpha_mode = AlphaMode::Opaque;
        float alpha_cutoff = 1.0f;
        float metallic_factor = 1.0f;
        float roughness_factor = 1.0f;
        glm::vec4 base_color_factor = glm::vec4(1.0f);
        glm::vec4 emissive_factor = glm::vec4(1.0f);

        Texture* base_color;
        Texture* metallic_roughness;
        Texture* normal;
        Texture* occlusion;
        Texture* emissive;

        struct TexCoordSets
        {
            uint8_t base_color = 0;
            uint8_t metallic_roughness = 0;
            uint8_t specular_glosiness = 0;
            uint8_t normal = 0;
            uint8_t occlusion = 0;
            uint8_t emissive = 0;
        } tex_coord_sets;

        struct Extension
        {
            Texture* specular_glosiness;
            Texture* diffuse;
            glm::vec4 diffuse_factor = glm::vec4(1.0f);
            glm::vec3 specular_factor = glm::vec3(0.0f);
        } extension;

        PbrWorkflow workflow = PbrWorkflow::MetallicRoughness;

        vk::DescriptorSet desc_set;
    };

    struct PushConstBlockMaterial
    {
        glm::vec4 base_color_factor;
        glm::vec4 emissive_facotr;
        glm::vec4 diffuse_factor;
        glm::vec4 specular_factor;
        float workflow;
        int color_texture_set;
        int physical_descriptor_texture_set;
        int normal_texture_set;
        int occlusion_texture_set;
        int emissive_texture_set;
        float metallic_facotr;
        float roughness_factor;
        float alpha_mask;
        float alpha_mask_cutoff;
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

        glm::mat4 matrix{ 1.0f };
        glm::vec3 translation;
        glm::vec3 scale{ 1.0f };
        glm::mat4 rotation{ 1.0f };

        glm::mat4 local_matrix()
        {
            return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;
        }

        glm::mat4 get_matrix()
        {
            glm::mat4 m = local_matrix();
            auto p = parent;
            while (p)
            {
                m = p->local_matrix() * m;
                p = p->parent;
            }
            return m;
        }

        void update();
        void setup_node_descriptor_set(vk::UniqueDescriptorPool& desc_pool, vk::UniqueDescriptorSetLayout& desc_set_layout, vk::Device& device);

        void draw(vk::CommandBuffer& cmd, vk::PipelineLayout& pipeline_layout, vk::DescriptorSet& desc_set) const;
    };

    struct Model
    {
        Model(std::string path, VulkanContext& ctx);
        ~Model() = default;

        void load_textures();
        void load_samplers();
        void load_materials();
        void load_meshes();
        Node load_node(size_t i);
        void load_nodes();
        void free();

        void draw(vk::CommandBuffer& cmd, vk::PipelineLayout& pipeline_layout, vk::DescriptorSet& desc_set) const;

        VulkanContext& ctx;
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
