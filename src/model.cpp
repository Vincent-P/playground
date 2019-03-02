#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>


#include "buffer.hpp"
#include "image.hpp"
#include "model.hpp"
#include "tiny_gltf.h"
#include "vulkan_context.hpp"

namespace my_app
{

    Texture::Texture(VulkanContext& ctx, tinygltf::Image& gltf_image, TextureSampler& texture_sampler)
    {
        auto& pixels = gltf_image.image;

        // Make a new vector with 4 components
        if (gltf_image.component == 3)
        {
            auto& old_pixels = pixels;
            size_t pixel_count = gltf_image.width * gltf_image.height;
            pixels = std::vector<unsigned char>(pixel_count * 4);
            for (size_t i = 0; i < pixel_count; i++)
            {
                pixels[4 * i] = old_pixels[3 * i];
                pixels[4 * i + 1] = old_pixels[3 * i + 1];
                pixels[4 * i + 2] = old_pixels[3 * i + 2];
                pixels[4 * i + 3] = 0;
            }
        }

        vk::Format format = vk::Format::eA8B8G8R8UnormPack32;
        width = gltf_image.width;
        height = gltf_image.height;
        mip_levels = std::floor(std::log2(std::max(width, height))) + 1.0;
        auto copy_queue = ctx.device->getQueue(ctx.graphics_family_idx, 0);

        // Move the pixels to a staging buffer that will be copied to the image
        Buffer staging_buffer{ctx.allocator, pixels.size(), vk::BufferUsageFlagBits::eTransferSrc};
        void* mapped = staging_buffer.Map();
        memcpy(mapped, pixels.data(), pixels.size());
        staging_buffer.Unmap();

        // Create the image that will contain the texture
        auto ci = vk::ImageCreateInfo();
        ci.imageType = vk::ImageType::e2D;
        ci.format = format;
        ci.mipLevels = mip_levels;
        ci.arrayLayers = 1;
        ci.samples = vk::SampleCountFlagBits::e1;
        ci.tiling = vk::ImageTiling::eOptimal;
        ci.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc;
        ci.sharingMode = vk::SharingMode::eExclusive;
        ci.initialLayout = vk::ImageLayout::eUndefined;
        ci.extent.width = width;
        ci.extent.height = height;
        ci.extent.depth = 1;
        image = Image{ctx.allocator, ci};

        // Copy buffer to the image
        vk::CommandBuffer cmd = ctx.device->allocateCommandBuffers({ctx.command_pool, vk::CommandBufferLevel::ePrimary, 1})[0];
        vk::CommandBufferBeginInfo cbi{};
        cmd.begin(cbi);

        vk::ImageSubresourceRange subresource_range(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

        {
            vk::ImageMemoryBarrier b{};
            b.oldLayout = vk::ImageLayout::eUndefined;
            b.newLayout = vk::ImageLayout::eTransferDstOptimal;
            b.srcAccessMask = {};
            b.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
            b.image = image.GetImage();
            b.subresourceRange = subresource_range;
            cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, nullptr, nullptr, b);
        }

        vk::BufferImageCopy region{};
        region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageExtent.width = width;
        region.imageExtent.height = height;
        region.imageExtent.depth = 1;
        cmd.copyBufferToImage(staging_buffer.GetBuffer(), image.GetImage(), vk::ImageLayout::eTransferDstOptimal, region);

        {
            vk::ImageMemoryBarrier b{};
            b.oldLayout = vk::ImageLayout::eTransferDstOptimal;
            b.newLayout = vk::ImageLayout::eTransferSrcOptimal;
            b.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            b.dstAccessMask = vk::AccessFlagBits::eTransferRead;
            b.image = image.GetImage();
            b.subresourceRange = subresource_range;
            cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, nullptr, nullptr, b);
        }

        cmd.end();

        vk::Fence fence = ctx.device->createFence({});
        vk::SubmitInfo submit{};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;

        copy_queue.submit(submit, fence);
        ctx.device->waitForFences(fence, VK_TRUE, UINT64_MAX);
        ctx.device->destroy(fence);

        staging_buffer.Free();

        // Generate the mipchain (because glTF's textures are regular images)

        vk::CommandBuffer bcmd = ctx.device->allocateCommandBuffers({ctx.command_pool, vk::CommandBufferLevel::ePrimary, 1})[0];
        bcmd.begin(cbi);

        for (uint32_t i = 1; i < mip_levels; i++)
        {
            vk::ImageBlit blit{};
            blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
            blit.srcSubresource.layerCount = 1;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcOffsets[1].x = width >> (i - 1);
            blit.srcOffsets[1].y = height >> (i - 1);
            blit.srcOffsets[1].z = 1;
            blit.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
            blit.dstSubresource.layerCount = 1;
            blit.dstSubresource.mipLevel = i;
            blit.dstOffsets[1].x = width >> i;
            blit.dstOffsets[1].y = height >> i;
            blit.dstOffsets[1].z = 1;

            vk::ImageSubresourceRange mip_sub_range(vk::ImageAspectFlagBits::eColor, i, 1, 0, 1);

            {
                vk::ImageMemoryBarrier b{};
                b.oldLayout = vk::ImageLayout::eUndefined;
                b.newLayout = vk::ImageLayout::eTransferDstOptimal;
                b.srcAccessMask = vk::AccessFlags();
                b.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
                b.image = image.GetImage();
                b.subresourceRange = mip_sub_range;
                bcmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), nullptr, nullptr, b);
            }

            bcmd.blitImage(image.GetImage(), vk::ImageLayout::eTransferSrcOptimal, image.GetImage(), vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eLinear);

            {
                vk::ImageMemoryBarrier b{};
                b.oldLayout = vk::ImageLayout::eTransferDstOptimal;
                b.newLayout = vk::ImageLayout::eTransferSrcOptimal;
                b.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
                b.dstAccessMask = vk::AccessFlagBits::eTransferRead;
                b.image = image.GetImage();
                b.subresourceRange = mip_sub_range;
                bcmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), nullptr, nullptr, b);
            }
        }

        bcmd.end();

        fence = ctx.device->createFence({});
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &bcmd;

        copy_queue.submit(submit, fence);
        ctx.device->waitForFences(fence, VK_TRUE, UINT64_MAX);
        ctx.device->destroy(fence);

        subresource_range.levelCount = mip_levels;
        desc_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        bcmd.begin(cbi);

        {
            vk::ImageMemoryBarrier b{};
            b.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
            b.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            b.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            b.dstAccessMask = vk::AccessFlagBits::eTransferRead;
            b.image = image.GetImage();
            b.subresourceRange = subresource_range;
            bcmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags(), nullptr, nullptr, b);
        }

        bcmd.end();

        fence = ctx.device->createFence({});
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &bcmd;

        copy_queue.submit(submit, fence);
        ctx.device->waitForFences(fence, VK_TRUE, UINT64_MAX);
        ctx.device->destroy(fence);

        // Create the sampler for the texture
        vk::SamplerCreateInfo sci{};
        sci.magFilter = texture_sampler.magFilter;
        sci.minFilter = texture_sampler.minFilter;
        sci.mipmapMode = vk::SamplerMipmapMode::eLinear;
        sci.addressModeU = texture_sampler.addressModeU;
        sci.addressModeV = texture_sampler.addressModeV;
        sci.addressModeW = texture_sampler.addressModeW;
        sci.compareOp = vk::CompareOp::eNever;
        sci.borderColor = vk::BorderColor::eFloatOpaqueWhite;
        sci.minLod = 0;
        sci.maxLod = float(mip_levels);
        sci.maxAnisotropy = 8.0f;
        sci.anisotropyEnable = VK_TRUE;
        desc_info.sampler = ctx.device->createSampler(sci);

        // Create the image view holding the texture
        vk::ImageViewCreateInfo vci{};
        vci.flags = {};
        vci.image = image.GetImage();
        vci.format = format;
        vci.components = {vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA};
        vci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        vci.subresourceRange.baseMipLevel = 0;
        vci.subresourceRange.levelCount = mip_levels;
        vci.subresourceRange.baseArrayLayer = 0;
        vci.subresourceRange.layerCount = 1;
        vci.viewType = vk::ImageViewType::e2D;
        desc_info.imageView = ctx.device->createImageView(vci);
    }

    Mesh::Mesh(VulkanContext& ctx)
        : uniform(ctx.allocator,
                  sizeof(UniformBlock),
                  vk::BufferUsageFlagBits::eUniformBuffer)
    {
    }

    void Mesh::draw(vk::CommandBuffer& cmd, vk::PipelineLayout& pipeline_layout, vk::DescriptorSet& desc_set) const
    {
        for (const auto& primitive : primitives)
        {
            const std::vector<vk::DescriptorSet> sets =
                {
                    desc_set,
                    primitive.material.desc_set,
                    uniform_desc};

            cmd.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                pipeline_layout,
                0,
                sets,
                nullptr);

            // Pass material parameters as push constants
            PushConstBlockMaterial pushConstBlockMaterial{};
            pushConstBlockMaterial.emissiveFactor = primitive.material.emissiveFactor;
            pushConstBlockMaterial.colorTextureSet = primitive.material.baseColorTexture != nullptr ? primitive.material.texCoordSets.baseColor : -1;
            pushConstBlockMaterial.normalTextureSet = primitive.material.normalTexture != nullptr ? primitive.material.texCoordSets.normal : -1;
            pushConstBlockMaterial.occlusionTextureSet = primitive.material.occlusionTexture != nullptr ? primitive.material.texCoordSets.occlusion : -1;
            pushConstBlockMaterial.emissiveTextureSet = primitive.material.emissiveTexture != nullptr ? primitive.material.texCoordSets.emissive : -1;
            pushConstBlockMaterial.alphaMask = static_cast<float>(primitive.material.alphaMode == Material::AlphaMode::Mask);
            pushConstBlockMaterial.alphaMaskCutoff = primitive.material.alphaCutoff;

            // TODO: glTF specs states that metallic roughness should be preferred, even if specular glosiness is present

            if (primitive.material.workflow == Material::PbrWorkflow::MetallicRoughness)
            {
                pushConstBlockMaterial.baseColorFactor = primitive.material.baseColorFactor;
                pushConstBlockMaterial.metallicFactor = primitive.material.metallicFactor;
                pushConstBlockMaterial.roughnessFactor = primitive.material.roughnessFactor;
                pushConstBlockMaterial.PhysicalDescriptorTextureSet = primitive.material.metallicRoughnessTexture != nullptr ? primitive.material.texCoordSets.metallicRoughness : -1;
                pushConstBlockMaterial.colorTextureSet = primitive.material.baseColorTexture != nullptr ? primitive.material.texCoordSets.baseColor : -1;
            }

            if (primitive.material.workflow == Material::PbrWorkflow::SpecularGlossiness)
            {
                pushConstBlockMaterial.PhysicalDescriptorTextureSet = primitive.material.extension.specularGlossinessTexture != nullptr ? primitive.material.texCoordSets.specularGlossiness : -1;
                pushConstBlockMaterial.colorTextureSet = primitive.material.extension.diffuseTexture != nullptr ? primitive.material.texCoordSets.baseColor : -1;
                pushConstBlockMaterial.diffuseFactor = primitive.material.extension.diffuseFactor;
                pushConstBlockMaterial.specularFactor = glm::vec4(primitive.material.extension.specularFactor, 1.0f);
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
        : ctx(ctx)
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

        LoadSamplers();
        LoadTextures();
        LoadMaterials();
        LoadMeshes();
        LoadNodes();

        for (auto& node : scene_nodes)
            node.update();
    }

    void Model::Free()
    {
        for (auto& mesh : meshes)
            mesh.uniform.Free();

        for (auto& text : textures)
        {
            text.image.Free();
            ctx.device->destroy(text.desc_info.imageView);
            ctx.device->destroy(text.desc_info.sampler);
        }

    }

    void Model::LoadTextures()
    {
        for (auto& texture : model.textures)
        {
            auto image = model.images[texture.source];
            TextureSampler texture_sampler{};
            if (texture.sampler > -1)
                texture_sampler = text_samplers[texture.sampler];
            textures.emplace_back(ctx, image, texture_sampler);
        }
    }

    void Model::LoadSamplers()
    {
        for (auto& sampler : model.samplers)
        {
            TextureSampler s{};
            s.minFilter = GetVkFilterMode(sampler.minFilter);
            s.magFilter = GetVkFilterMode(sampler.magFilter);
            s.addressModeU = GetVkWrapMode(sampler.wrapS);
            s.addressModeV = GetVkWrapMode(sampler.wrapT);
            s.addressModeW = s.addressModeV;
            text_samplers.push_back(std::move(s));
        }
    }

    void Model::LoadMaterials()
    {
        for (auto& material : model.materials)
        {
            Material new_mat{};

            if (material.values.count("metallicFactor"))
                new_mat.metallicFactor = material.values["metallicFactor"].Factor();

            if (material.values.count("roughnessFactor"))
                new_mat.roughnessFactor = material.values["roughnessFactor"].Factor();

            if (material.values.count("baseColorFactor"))
                new_mat.baseColorFactor = glm::make_vec4(material.values["baseColorFactor"].ColorFactor().data());

            if (material.values.count("baseColorTexture"))
            {
                new_mat.baseColorTexture = &textures[material.values["baseColorTexture"].TextureIndex()];
                new_mat.texCoordSets.baseColor = material.values["baseColorTexture"].TextureTexCoord();
            }

            if (material.values.count("metallicRoughnessTexture"))
            {
                new_mat.metallicRoughnessTexture = &textures[material.values["metallicRoughnessTexture"].TextureIndex()];
                new_mat.texCoordSets.metallicRoughness = material.values["metallicRoughnessTexture"].TextureTexCoord();
            }

            if (material.additionalValues.count("normalTexture"))
            {
                new_mat.normalTexture = &textures[material.additionalValues["normalTexture"].TextureIndex()];
                new_mat.texCoordSets.normal = material.additionalValues["normalTexture"].TextureTexCoord();
            }

            if (material.additionalValues.count("emissiveTexture"))
            {
                new_mat.emissiveTexture = &textures[material.additionalValues["emissiveTexture"].TextureIndex()];
                new_mat.texCoordSets.emissive = material.additionalValues["emissiveTexture"].TextureTexCoord();
            }

            if (material.additionalValues.count("occlusionTexture"))
            {
                new_mat.occlusionTexture = &textures[material.additionalValues["occlusionTexture"].TextureIndex()];
                new_mat.texCoordSets.occlusion = material.additionalValues["occlusionTexture"].TextureTexCoord();
            }

            if (material.additionalValues.count("alphaMode"))
            {
                tinygltf::Parameter& param = material.additionalValues["alphaMode"];
                if (param.string_value == "BLEND")
                {
                    new_mat.alphaMode = Material::AlphaMode::Blend;
                }
                if (param.string_value == "MASK")
                {
                    new_mat.alphaCutoff = 0.5f;
                    new_mat.alphaMode = Material::AlphaMode::Mask;
                }
            }

            if (material.additionalValues.count("alphaCutoff"))
                new_mat.alphaCutoff = material.additionalValues["alphaCutoff"].Factor();

            if (material.additionalValues.count("emissiveFactor"))
                new_mat.emissiveFactor = glm::vec4(glm::make_vec3(material.additionalValues["emissiveFactor"].ColorFactor().data()), 1.0f);

            materials.push_back(std::move(new_mat));
        }

        // Add a default material at the end for primitive without materials
        materials.emplace_back();
    }

    void Model::LoadMeshes()
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
                        Vertex vertex{};
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

                    auto total_offset = normal_acc.byteOffset + normal_view.byteOffset;
                    auto data = reinterpret_cast<float*>(&(model.buffers[normal_view.buffer].data[total_offset]));

                    for (size_t i = 0; i < normal_acc.count; i++)
                        vertices[first_vertex + i].normal = glm::make_vec3(&data[i * 3]);
                }


                auto texcoord0 = attrs.find("TEXCOORD_0");
                if (texcoord0 != attrs.end())
                {
                    const auto& uv_acc = model.accessors[texcoord0->second];
                    const auto& uv_view = model.bufferViews[uv_acc.bufferView];

                    auto total_offset = uv_acc.byteOffset + uv_view.byteOffset;
                    auto data = reinterpret_cast<const float*>(&(model.buffers[uv_view.buffer].data[total_offset]));

                    for (size_t i = 0; i < uv_acc.count; i++)
                        vertices[first_vertex + i].uv0 = glm::make_vec2(&data[i * 2]);
                }

                auto texcoord1 = attrs.find("TEXCOORD_1");
                if (texcoord1 != attrs.end())
                {
                    const auto& uv_acc = model.accessors[texcoord1->second];
                    const auto& uv_view = model.bufferViews[uv_acc.bufferView];

                    auto total_offset = uv_acc.byteOffset + uv_view.byteOffset;
                    auto data = reinterpret_cast<const float*>(&(model.buffers[uv_view.buffer].data[total_offset]));

                    for (size_t i = 0; i < uv_acc.count; i++)
                        vertices[first_vertex + i].uv1 = glm::make_vec2(&data[i * 2]);
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
                Primitive p{first_vertex, first_index, index_count, prim_mat};
                m.primitives.push_back(std::move(p));
            }

            meshes.push_back(std::move(m));
        }
    }

    Node Model::LoadNode(size_t i)
    {
        auto& node = model.nodes[i];
        Node n{};

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
