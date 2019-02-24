#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "model.hpp"
#include "tiny_gltf.h"

namespace my_app
{
    void Mesh::draw(vk::CommandBuffer& cmd, vk::PipelineLayout& pipeline_layout) const
    {
        for (const auto& primitive : primitives)
        {
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

            if (primitive.material.workflow == Material::PbrWorkflow::MetallicRoughness) {
                pushConstBlockMaterial.baseColorFactor = primitive.material.baseColorFactor;
                pushConstBlockMaterial.metallicFactor = primitive.material.metallicFactor;
                pushConstBlockMaterial.roughnessFactor = primitive.material.roughnessFactor;

                // should check if a texture is assigned
                pushConstBlockMaterial.hasPhysicalDescriptorTexture = static_cast<float>(false);
                pushConstBlockMaterial.hasColorTexture = static_cast<float>(false);
            }

            if (primitive.material.workflow == Material::PbrWorkflow::SpecularGlossiness) {
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

    Model::Model(std::string path)
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
        LoadMeshesBuffers();
    }

    void Model::LoadMaterials()
    {
        for (auto& material: model.materials)
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

    void Model::LoadMeshesBuffers()
    {
        for (const auto& mesh : model.meshes)
        {
            Mesh m;

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

                {
                    const auto& position_acc = model.accessors[position->second];
                    const auto& position_view = model.bufferViews[position_acc.bufferView];
                    auto total_offset = position_acc.byteOffset + position_view.byteOffset;

                    auto prim_vertices = reinterpret_cast<float*>(
                        &(model.buffers[position_view.buffer].data[total_offset]));

                    for (size_t i = 0; i < position_acc.count; i++)
                    {
                        Vertex vertex;
                        vertex.pos = glm::make_vec3(&prim_vertices[i * 3]);
                        vertices.push_back(std::move(vertex));
                    }
                }

                auto normal = attrs.find("NORMAL");
                if (normal != attrs.end())
                {
                    const auto& normal_acc = model.accessors[normal->second];
                    const auto& normal_view = model.bufferViews[normal_acc.bufferView];
                    auto total_offset = normal_acc.byteOffset + normal_view.byteOffset;

                    auto prim_vertices = reinterpret_cast<float*>(
                        &(model.buffers[normal_view.buffer].data[total_offset]));

                    for (size_t i = 0; i < normal_acc.count; i++)
                    {
                        vertices[i].normal = glm::make_vec3(&prim_vertices[i * 3]);
                    }
                }

                // Load vertices' index
                {
                    const auto& indices_acc = model.accessors[primitive.indices];
                    const auto& indices_view = model.bufferViews[indices_acc.bufferView];
                    auto total_offset = indices_acc.byteOffset + indices_view.byteOffset;
                    index_count = indices_acc.count;

                    switch (indices_acc.componentType)
                    {
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        {
                            auto prim_indices = reinterpret_cast<uint16_t*>(
                                &(model.buffers[indices_view.buffer].data[total_offset]));

                            for (size_t i = 0; i < indices_acc.count; i++)
                            {
                                indices.push_back(prim_indices[i]);
                            }
                        }
                        break;
                        default:
                            break;
                    }
                }

                auto& prim_mat = primitive.material > -1 ? materials[primitive.material] : materials.back();
                Primitive p = {first_vertex, first_index, index_count, prim_mat};
                m.primitives.push_back(std::move(p));
            }

            meshes.push_back(std::move(m));
        }
    }

    void Model::draw(vk::CommandBuffer& cmd, vk::PipelineLayout& pipeline_layout) const
    {
        for (const auto& mesh : meshes)
        {
            mesh.draw(cmd, pipeline_layout);
        }
    }
}    // namespace my_app
