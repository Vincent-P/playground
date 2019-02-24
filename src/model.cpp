#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>


#include "model.hpp"
#include "tiny_gltf.h"
#include "vulkan_context.hpp"

namespace my_app
{
    Mesh::Mesh(VulkanContext& ctx)
        : uniform(sizeof(UniformBlock),
                  vk::BufferUsageFlagBits::eUniformBuffer,
                  VMA_MEMORY_USAGE_CPU_TO_GPU,
                  ctx.allocator)
    {
    }

    void Mesh::draw(vk::CommandBuffer& cmd, vk::PipelineLayout& pipeline_layout, vk::DescriptorSet& desc_set) const
    {
        for (const auto& primitive : primitives)
        {
            const std::vector<vk::DescriptorSet> sets =
                {
                    desc_set,
                    uniform_desc,

                };

            cmd.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                pipeline_layout,
                0,
                sets.size(),
                sets.data(),
                0,
                nullptr);

            // Pass material parameters as push constants
            PushConstBlockMaterial pushConstBlockMaterial{};
            pushConstBlockMaterial.emissiveFactor = primitive.material.emissiveFactor;
            // pushConstBlockMaterial.hasNormalTexture = static_cast<float>(primitive.material.normalTexture != nullptr);
            // pushConstBlockMaterial.hasOcclusionTexture = static_cast<float>(primitive.material.occlusionTexture != nullptr);
            // pushConstBlockMaterial.hasEmissiveTexture = static_cast<float>(primitive.material.emissiveTexture != nullptr);
            pushConstBlockMaterial.alphaMask = static_cast<float>(primitive.material.alphaMode == Material::AlphaMode::Mask);
            pushConstBlockMaterial.alphaMaskCutoff = primitive.material.alphaCutoff;
            pushConstBlockMaterial.workflow = Material::WorkflowFloat(primitive.material.workflow);

            // TODO: glTF specs states that metallic roughness should be preferred, even if specular glosiness is present

            if (primitive.material.workflow == Material::PbrWorkflow::MetallicRoughness)
            {
                pushConstBlockMaterial.baseColorFactor = primitive.material.baseColorFactor;
                pushConstBlockMaterial.metallicFactor = primitive.material.metallicFactor;
                pushConstBlockMaterial.roughnessFactor = primitive.material.roughnessFactor;

                // should check if a texture is assigned
                pushConstBlockMaterial.hasPhysicalDescriptorTexture = static_cast<float>(false);
                pushConstBlockMaterial.hasColorTexture = static_cast<float>(false);
            }

            if (primitive.material.workflow == Material::PbrWorkflow::SpecularGlossiness)
            {
                pushConstBlockMaterial.diffuseFactor = primitive.material.extension.diffuseFactor;
                pushConstBlockMaterial.specularFactor = glm::vec4(primitive.material.extension.specularFactor, 1.0f);

                // should check if a texture is assigned
                pushConstBlockMaterial.hasPhysicalDescriptorTexture = static_cast<float>(false);
                pushConstBlockMaterial.hasColorTexture = static_cast<float>(false);
            }

            cmd.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstBlockMaterial), &pushConstBlockMaterial);

            cmd.drawIndexed(primitive.index_count,
                            1,
                            primitive.first_index,
                            primitive.first_vertex,
                            0);
        }
    }

    void Node::SetupNodeDescriptorSet(vk::DescriptorPool& desc_pool, vk::DescriptorSetLayout& desc_set_layout, vk::Device& device)
    {
        if (mesh)
        {
            vk::DescriptorSetAllocateInfo allocInfo{};
            allocInfo.descriptorPool = desc_pool;
            allocInfo.pSetLayouts = &desc_set_layout;
            allocInfo.descriptorSetCount = 1;

            mesh->uniform_desc = device.allocateDescriptorSets(allocInfo).front();

            vk::WriteDescriptorSet write{};
            auto bdi = mesh->uniform.GetDescInfo();
            write.descriptorType = vk::DescriptorType::eUniformBuffer;
            write.descriptorCount = 1;
            write.dstSet = mesh->uniform_desc;
            write.dstBinding = 0;
            write.pBufferInfo = &bdi;

            device.updateDescriptorSets(write, nullptr);
        }

        for (auto& child : children)
            child.SetupNodeDescriptorSet(desc_pool, desc_set_layout, device);
    }

    void Node::update()
    {
        if (mesh)
        {
            glm::mat4 m = getMatrix();
            void* mapped = mesh->uniform.Map();
            memcpy(mapped, &m, sizeof(m));
            mesh->uniform.Unmap();
        }

        for (auto& child : children)
            child.update();
    }


    void Node::draw(vk::CommandBuffer& cmd, vk::PipelineLayout& pipeline_layout, vk::DescriptorSet& desc_set) const
    {
        if (mesh)
            mesh->draw(cmd, pipeline_layout, desc_set);

        for (const auto& node : children)
            node.draw(cmd, pipeline_layout, desc_set);
    }

    Model::Model(std::string path, VulkanContext& ctx)
    {
        std::string err, warn;
        if (!loader.LoadASCIIFromFile(&model, &err, &warn, path) || !err.empty() || !warn.empty())
        {
            if (!err.empty())
                std::clog << err;
            if (!warn.empty())
                std::clog << warn;
            throw std::runtime_error("Failed to load model.");
        }

        LoadMaterials();
        LoadMeshes(ctx);
        LoadNodes();

        for (auto& node : scene_nodes)
            node.update();
    }

    void Model::Free()
    {
        for (auto& mesh : meshes)
            mesh.uniform.Free();
    }

    void Model::LoadMaterials()
    {
        for (auto& material : model.materials)
        {
            Material new_mat;

            if (material.values.count("baseColorFactor"))
                new_mat.baseColorFactor = glm::make_vec4(material.values["baseColorFactor"].ColorFactor().data());
            if (material.values.count("metallicFactor"))
                new_mat.metallicFactor = material.values["metallicFactor"].Factor();

            materials.push_back(std::move(new_mat));
        }

        // Add a default material at the end for primitive without materials
        materials.emplace_back();
    }

    void Model::LoadMeshes(VulkanContext& ctx)
    {
        for (const auto& mesh : model.meshes)
        {
            Mesh m{ctx};

            for (const auto& primitive : mesh.primitives)
            {
                std::uint32_t first_vertex = vertices.size();
                std::uint32_t first_index = indices.size();
                std::uint32_t index_count = 0;

                // Load vertices position
                const auto& attrs = primitive.attributes;

                auto position = attrs.find("POSITION");
                if (position == attrs.end())
                    throw std::runtime_error("The mesh doesn't have vertex positions.");

                    const auto& position_acc = model.accessors[position->second];
                    const auto& position_view = model.bufferViews[position_acc.bufferView];

                {
                    auto total_offset = position_acc.byteOffset + position_view.byteOffset;
                    auto data = reinterpret_cast<float*>(&(model.buffers[position_view.buffer].data[total_offset]));

                    for (size_t i = 0; i < position_acc.count; i++)
                    {
                        Vertex vertex;
                        vertex.pos = glm::make_vec3(&data[i * 3]);
                        vertex.normal = glm::vec3(0.0f);
                        vertices.push_back(std::move(vertex));
                    }
                }

                auto normal = attrs.find("NORMAL");
                if (normal != attrs.end())
                {
                    const auto& normal_acc = model.accessors[normal->second];
                    const auto& normal_view = model.bufferViews[normal_acc.bufferView];

                    assert(normal_acc.count == position_acc.count);

                    auto total_offset = normal_acc.byteOffset + normal_view.byteOffset;
                    auto data = reinterpret_cast<float*>(&(model.buffers[normal_view.buffer].data[total_offset]));

                    for (size_t i = 0; i < normal_acc.count; i++)
                        vertices[first_vertex + i].normal = glm::make_vec3(&data[i * 3]);
                }

                // Load vertices' index
                {
                    const auto& indices_acc = model.accessors[primitive.indices];
                    const auto& indices_view = model.bufferViews[indices_acc.bufferView];
                    auto total_offset = indices_acc.byteOffset + indices_view.byteOffset;
                    index_count = indices_acc.count;

                    auto data = &(model.buffers[indices_view.buffer].data[total_offset]);

                    switch (indices_acc.componentType)
                    {
                        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
                        {
                            auto prim_indices = reinterpret_cast<uint32_t*>(data);
                            for (size_t i = 0; i < indices_acc.count; i++)
                                indices.push_back(prim_indices[i]);
                            break;
                        }
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        {
                            auto prim_indices = reinterpret_cast<uint16_t*>(data);
                            for (size_t i = 0; i < indices_acc.count; i++)
                                indices.push_back(prim_indices[i]);
                            break;
                        }
                        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
                        {
                            auto prim_indices = reinterpret_cast<uint8_t*>(data);
                            for (size_t i = 0; i < indices_acc.count; i++)
                                indices.push_back(prim_indices[i]);
                            break;
                        }
                        default:
                            std::cerr << "Index component type " << indices_acc.componentType << " not supported!" << std::endl;
                            throw std::runtime_error("Unsupported index component type.");
                    }
                }

                auto& prim_mat = primitive.material > -1 ? materials[primitive.material] : materials.back();
                Primitive p = {first_vertex, first_index, index_count, prim_mat};
                m.primitives.push_back(std::move(p));
            }

            meshes.push_back(std::move(m));
        }
    }

    Node Model::LoadNode(size_t i)
    {
        auto& node = model.nodes[i];
        Node n;

        n.matrix = glm::mat4(1.0f);

        // Generate local node matrix
        if (node.translation.size() == 3)
            n.translation = glm::make_vec3(node.translation.data());
        n.translation *= global_scale;

        if (node.rotation.size() == 4)
        {
            glm::quat q = glm::make_quat(node.rotation.data());
            n.rotation = glm::mat4(q);
        }

        if (node.scale.size() == 3)
            n.scale = glm::make_vec3(node.scale.data());
        n.scale *= global_scale;

        if (node.matrix.size() == 16)
            n.matrix = glm::make_mat4x4(node.matrix.data());

        if (node.mesh > -1)
            n.mesh = &meshes[node.mesh];

        n.children.resize(node.children.size());
        for (size_t j = 0; j < node.children.size(); j++)
        {
            n.children[j].parent = &n;
            n.children[j] = LoadNode(node.children[j]);
        }

        return n;
    }

    void Model::LoadNodes()
    {
        const auto& scene = model.scenes[model.defaultScene > -1 ? model.defaultScene : 0];
        scene_nodes.resize(scene.nodes.size());

        for (size_t i = 0; i < scene.nodes.size(); i++)
            scene_nodes[i] = LoadNode(scene.nodes[i]);
    }

    void Model::draw(vk::CommandBuffer& cmd, vk::PipelineLayout& pipeline_layout, vk::DescriptorSet& desc_set) const
    {
        // TODO(vincent): bind vertex and index buffer of the model
        for (const auto& node : scene_nodes)
            node.draw(cmd, pipeline_layout, desc_set);
    }
}    // namespace my_app
