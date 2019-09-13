#include "model.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <tinygltf/tiny_gltf.h>
#include <thsvs/thsvs_simpler_vulkan_synchronization.h>

#include "buffer.hpp"
#include "image.hpp"
#include "timer.hpp"
#include "tools.hpp"
#include "vulkan_context.hpp"

namespace my_app
{

    Texture::Texture(const VulkanContext& ctx, tinygltf::Image& gltf_image, TextureSampler& sampler)
    {
        auto& pixels = gltf_image.image;

        // Make a new vector with 4 components
        if (gltf_image.component == 3)
        {
            auto& old_pixels = pixels;
            size_t pixel_count = static_cast<size_t>(gltf_image.width * gltf_image.height);
            pixels = std::vector<unsigned char>(pixel_count * 4);
            for (size_t i = 0; i < pixel_count; i++)
            {
                pixels[4 * i] = old_pixels[3 * i];
                pixels[4 * i + 1] = old_pixels[3 * i + 1];
                pixels[4 * i + 2] = old_pixels[3 * i + 2];
                pixels[4 * i + 3] = 0;
            }
        }

        vk::Format format = vk::Format::eR8G8B8A8Unorm;
        width = static_cast<uint32_t>(gltf_image.width);
        height = static_cast<uint32_t>(gltf_image.height);
        mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height))) + 1.0);
        layer_count = 0;

        // Create the image that will contain the texture
        vk::ImageCreateInfo ci{};
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

        std::string texture_name = "Texture ";
        texture_name += gltf_image.name;
        image = Image{ texture_name, ctx.allocator, ci };

        vk::ImageSubresourceRange subresource_range;
        subresource_range.aspectMask = vk::ImageAspectFlagBits::eColor;
        subresource_range.baseArrayLayer = 0;
        subresource_range.layerCount = 1;
        subresource_range.baseMipLevel = 0;
        subresource_range.levelCount = 1;

        // Generate the mipchain (because glTF's textures are regular images)
        auto& cmd = ctx.texture_command_buffer;

        cmd->begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

        VulkanContext::CopyDataToImageParams params(image, subresource_range);
        params.data = pixels.data();
        params.data_size = pixels.size();
        params.current_image_access = THSVS_ACCESS_NONE;
        params.height = height;
        params.width = width;
        params.next_image_access = THSVS_ACCESS_TRANSFER_READ;
        auto staging1 = ctx.copy_data_to_image_cmd(cmd.get(), params);

        for (uint32_t i = 1; i < mip_levels; i++)
        {
            subresource_range.baseMipLevel = i;

            // We are going to write to this mip level
            ctx.transition_layout_cmd(*cmd, image.get_image(), THSVS_ACCESS_NONE, THSVS_ACCESS_TRANSFER_WRITE, subresource_range);

            auto src_width = width >> (i - 1);
            auto src_height = height >> (i - 1);
            auto dst_width = width >> i;
            auto dst_height = height >> i;
            vk::ImageBlit blit{};
            blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
            blit.srcSubresource.layerCount = 1;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcOffsets[1].x = static_cast<int32_t>(src_width);
            blit.srcOffsets[1].y = static_cast<int32_t>(src_height);
            blit.srcOffsets[1].z = 1;
            blit.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
            blit.dstSubresource.layerCount = 1;
            blit.dstSubresource.mipLevel = i;
            blit.dstOffsets[1].x = static_cast<int32_t>(dst_width);
            blit.dstOffsets[1].y = static_cast<int32_t>(dst_height);
            blit.dstOffsets[1].z = 1;

            cmd->blitImage(image.get_image(), vk::ImageLayout::eTransferSrcOptimal, image.get_image(), vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eLinear);

            // This mip level will be read to create the next one
            ctx.transition_layout_cmd(*cmd, image.get_image(), THSVS_ACCESS_TRANSFER_WRITE, THSVS_ACCESS_TRANSFER_READ, subresource_range);
        }

        // Set the range to all the mip levels
        subresource_range.baseMipLevel = 0;
        subresource_range.levelCount = mip_levels;

        // Set the final layout to shader read
        ctx.transition_layout_cmd(*cmd, image.get_image(), THSVS_ACCESS_TRANSFER_READ, THSVS_ACCESS_ANY_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER, subresource_range);
        desc_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        cmd->end();

        auto fence = ctx.device->createFenceUnique({});
        vk::SubmitInfo submit{};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd.get();
        ctx.get_graphics_queue().submit(submit, fence.get());

        ctx.device->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);

        staging1.free();

        // Create the sampler for the texture
        vk::SamplerCreateInfo sci{};
        sci.magFilter = sampler.mag_filter;
        sci.minFilter = sampler.min_filter;
        sci.mipmapMode = vk::SamplerMipmapMode::eLinear;
        sci.addressModeU = sampler.address_mode_u;
        sci.addressModeV = sampler.address_mode_v;
        sci.addressModeW = sampler.address_mode_w;
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
        vci.image = image.get_image();
        vci.format = format;
        vci.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
        vci.subresourceRange = subresource_range;
        vci.viewType = vk::ImageViewType::e2D;
        desc_info.imageView = ctx.device->createImageView(vci);
    }


    Model::Model(std::string path, const VulkanContext& ctx)
    {
        std::string err, warn;
        tinygltf::Model model;
        if (!loader.LoadASCIIFromFile(&model, &err, &warn, path) || !err.empty() || !warn.empty())
        {
            if (!err.empty())
                std::clog << err;
            if (!warn.empty())
                std::clog << warn;
            throw std::runtime_error("Failed to load model.");
        }

        load_samplers(model);
        load_textures(model, ctx);
        load_materials(model);
        load_meshes(model);
        load_nodes(model);
    }

    void Model::free(const VulkanContext& ctx)
    {
        for (auto& text : textures)
        {
            text.image.free();
            ctx.device->destroy(text.desc_info.imageView);
            ctx.device->destroy(text.desc_info.sampler);
        }
    }

    void Model::load_textures(tinygltf::Model& model, const VulkanContext& ctx)
    {
        for (auto& texture : model.textures)
        {
            auto source_idx = static_cast<size_t>(texture.source);
            auto image = model.images[source_idx];
            TextureSampler texture_sampler{};
            if (texture.sampler > -1)
            {
                auto sampler_idx = static_cast<size_t>(texture.sampler);
                texture_sampler = text_samplers[sampler_idx];
            }
            textures.emplace_back(ctx, image, texture_sampler);
        }
    }

    void Model::load_samplers(tinygltf::Model& model)
    {
        for (auto& sampler : model.samplers)
        {
            TextureSampler s{};
            s.min_filter = get_vk_filter_mode(sampler.minFilter);
            s.mag_filter = get_vk_filter_mode(sampler.magFilter);
            s.address_mode_u = get_vk_wrap_mode(sampler.wrapS);
            s.address_mode_v = get_vk_wrap_mode(sampler.wrapT);
            s.address_mode_w = s.address_mode_v;
            text_samplers.push_back(std::move(s));
        }
    }

    void Model::load_materials(tinygltf::Model& model)
    {
        for (auto& material : model.materials)
        {
            Material new_mat{};

            if (material.values.count("metallicFactor"))
                new_mat.metallic_factor = static_cast<float>(material.values["metallicFactor"].Factor());

            if (material.values.count("roughnessFactor"))
                new_mat.roughness_factor = static_cast<float>(material.values["roughnessFactor"].Factor());

            if (material.values.count("baseColorFactor"))
                new_mat.base_color_factor = glm::make_vec4(material.values["baseColorFactor"].ColorFactor().data());

            if (material.values.count("baseColorTexture"))
            {
                new_mat.base_color = material.values["baseColorTexture"].TextureIndex();
                new_mat.tex_coord_sets.base_color = static_cast<uint8_t>(material.values["baseColorTexture"].TextureTexCoord());
            }

            if (material.values.count("metallicRoughnessTexture"))
            {
                new_mat.metallic_roughness = material.values["metallicRoughnessTexture"].TextureIndex();
                new_mat.tex_coord_sets.metallic_roughness = static_cast<uint8_t>(material.values["metallicRoughnessTexture"].TextureTexCoord());
            }

            if (material.additionalValues.count("normalTexture"))
            {
                new_mat.normal = material.additionalValues["normalTexture"].TextureIndex();
                new_mat.tex_coord_sets.normal = static_cast<uint8_t>(material.additionalValues["normalTexture"].TextureTexCoord());
            }

            if (material.additionalValues.count("emissiveTexture"))
            {
                new_mat.emissive = material.additionalValues["emissiveTexture"].TextureIndex();
                new_mat.tex_coord_sets.emissive = static_cast<uint8_t>(material.additionalValues["emissiveTexture"].TextureTexCoord());
            }

            if (material.additionalValues.count("occlusionTexture"))
            {
                new_mat.occlusion = material.additionalValues["occlusionTexture"].TextureIndex();
                new_mat.tex_coord_sets.occlusion = static_cast<uint8_t>(material.additionalValues["occlusionTexture"].TextureTexCoord());
            }

            if (material.additionalValues.count("alphaMode"))
            {
                tinygltf::Parameter& param = material.additionalValues["alphaMode"];
                if (param.string_value == "BLEND")
                {
                    new_mat.alpha_mode = Material::AlphaMode::Blend;
                }
                if (param.string_value == "MASK")
                {
                    new_mat.alpha_cutoff = 0.5f;
                    new_mat.alpha_mode = Material::AlphaMode::Mask;
                }
            }

            if (material.additionalValues.count("alphaCutoff"))
                new_mat.alpha_cutoff = static_cast<float>(material.additionalValues["alphaCutoff"].Factor());

            if (material.additionalValues.count("emissiveFactor"))
                new_mat.emissive_factor = glm::vec4(glm::make_vec3(material.additionalValues["emissiveFactor"].ColorFactor().data()), 1.0f);

            if (material.extensions.count("KHR_materials_pbrSpecularGlossiness"))
            {
                auto& extension = material.extensions.at("KHR_materials_pbrSpecularGlossiness");

                if (extension.Has("specularGlossinessTexture"))
                {
                    auto index = extension.Get("specularGlossinessTexture").Get("index");
                    new_mat.extension.specular_glosiness = index.Get<int>();
                    auto texCoordSet = extension.Get("specularGlossinessTexture").Get("texCoord");
                    new_mat.tex_coord_sets.specular_glosiness = static_cast<uint8_t>(texCoordSet.Get<int>());
                    new_mat.workflow = Material::PbrWorkflow::SpecularGlossiness;
                }
                if (extension.Has("diffuseTexture"))
                {
                    auto index = extension.Get("diffuseTexture").Get("index");
                    new_mat.extension.diffuse = index.Get<int>();
                }
                if (extension.Has("diffuseFactor"))
                {
                    auto factor = extension.Get("diffuseFactor");
                    for (int i = 0; static_cast<unsigned>(i) < factor.ArrayLen(); i++)
                    {
                        auto val = factor.Get(i);
                        new_mat.extension.diffuse_factor[i] = val.IsNumber() ? static_cast<float>(val.Get<double>()) : static_cast<float>(val.Get<int>());
                    }
                }
                if (extension.Has("specularFactor"))
                {
                    auto factor = extension.Get("specularFactor");
                    for (int i = 0; static_cast<unsigned>(i) < factor.ArrayLen(); i++)
                    {
                        auto val = factor.Get(i);
                        new_mat.extension.specular_factor[i] = val.IsNumber() ? static_cast<float>(val.Get<double>()) : static_cast<float>(val.Get<int>());
                    }
                }
            }

            materials.push_back(std::move(new_mat));
        }

        // Add a default material at the end for primitive without materials
        materials.emplace_back();
    }

    void Model::load_meshes(tinygltf::Model& model)
    {
        for (const auto& mesh : model.meshes)
        {
            Mesh m;

            for (const auto& primitive : mesh.primitives)
            {
                auto first_vertex = static_cast<uint32_t>(vertices.size());
                auto first_index = static_cast<uint32_t>(indices.size());
                std::uint32_t index_count = 0;

                // Load vertices position
                const auto& attrs = primitive.attributes;

                auto position = attrs.find("POSITION");
                if (position == attrs.end())
                    throw std::runtime_error("The mesh doesn't have vertex positions.");

                const auto& position_acc = model.accessors[static_cast<size_t>(position->second)];
                const auto& position_view = model.bufferViews[static_cast<size_t>(position_acc.bufferView)];

                {
                    auto total_offset = position_acc.byteOffset + position_view.byteOffset;
                    auto data = reinterpret_cast<float*>(&(model.buffers[static_cast<size_t>(position_view.buffer)].data[total_offset]));

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
                    const auto& normal_acc = model.accessors[static_cast<size_t>(normal->second)];
                    const auto& normal_view = model.bufferViews[static_cast<size_t>(normal_acc.bufferView)];

                    auto total_offset = normal_acc.byteOffset + normal_view.byteOffset;
                    auto data = reinterpret_cast<float*>(&(model.buffers[static_cast<size_t>(normal_view.buffer)].data[total_offset]));

                    for (size_t i = 0; i < normal_acc.count; i++)
                        vertices[first_vertex + i].normal = glm::make_vec3(&data[i * 3]);
                }


                auto texcoord0 = attrs.find("TEXCOORD_0");
                if (texcoord0 != attrs.end())
                {
                    const auto& uv_acc = model.accessors[static_cast<size_t>(texcoord0->second)];
                    const auto& uv_view = model.bufferViews[static_cast<size_t>(uv_acc.bufferView)];

                    auto total_offset = uv_acc.byteOffset + uv_view.byteOffset;
                    auto data = reinterpret_cast<const float*>(&(model.buffers[static_cast<size_t>(uv_view.buffer)].data[total_offset]));

                    for (size_t i = 0; i < uv_acc.count; i++)
                        vertices[first_vertex + i].uv0 = glm::make_vec2(&data[i * 2]);
                }

                auto texcoord1 = attrs.find("TEXCOORD_1");
                if (texcoord1 != attrs.end())
                {
                    const auto& uv_acc = model.accessors[static_cast<size_t>(texcoord1->second)];
                    const auto& uv_view = model.bufferViews[static_cast<size_t>(uv_acc.bufferView)];

                    auto total_offset = uv_acc.byteOffset + uv_view.byteOffset;
                    auto data = reinterpret_cast<const float*>(&(model.buffers[static_cast<size_t>(uv_view.buffer)].data[total_offset]));

                    for (size_t i = 0; i < uv_acc.count; i++)
                        vertices[first_vertex + i].uv1 = glm::make_vec2(&data[i * 2]);
                }

                // Load vertices' index
                {
                    const auto& indices_acc = model.accessors[static_cast<size_t>(primitive.indices)];
                    const auto& indices_view = model.bufferViews[static_cast<size_t>(indices_acc.bufferView)];
                    auto total_offset = indices_acc.byteOffset + indices_view.byteOffset;
                    index_count = static_cast<uint32_t>(indices_acc.count);

                    auto data = &(model.buffers[static_cast<size_t>(indices_view.buffer)].data[total_offset]);

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

                auto prim_mat = primitive.material > -1 ? static_cast<uint32_t>(primitive.material) : static_cast<uint32_t>(materials.size() - 1);
                Primitive p{ first_vertex, first_index, index_count, prim_mat };
                m.primitives.push_back(std::move(p));
            }

            meshes.push_back(std::move(m));
        }
    }

    Node Model::load_node(tinygltf::Model& model, size_t i)
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
            n.mesh = static_cast<uint32_t>(node.mesh);
        else
            n.mesh = Handle::h_null;

        n.children.resize(node.children.size());
        for (size_t j = 0; j < node.children.size(); j++)
        {
            n.children[j].parent = &n;
            n.children[j] = load_node(model, static_cast<size_t>(node.children[j]));
        }

        return n;
    }

    void Model::load_nodes(tinygltf::Model& model)
    {
        const auto& scene = model.scenes[model.defaultScene > -1 ? static_cast<size_t>(model.defaultScene) : 0];
        scene_nodes.resize(scene.nodes.size());

        for (size_t i = 0; i < scene.nodes.size(); i++)
            scene_nodes[i] = load_node(model, static_cast<size_t>(scene.nodes[i]));
    }

    static void draw_rec(const Model& model, const Node& node, vk::CommandBuffer cmd, vk::PipelineLayout pipeline_layout, const MultipleDescriptorSet& transforms_set, const MultipleDescriptorSet& materials_set)
    {
        if (node.mesh.is_valid())
        {
            // bind mesh descriptor
            const auto& mesh = model.meshes[node.mesh.index];

            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 2, transforms_set.descriptors[node.mesh.index].get(), nullptr);

            for (const auto& primitive : mesh.primitives)
            {
                // bind material descriptor
                const auto& material = model.materials[primitive.material];

                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 3, materials_set.descriptors[primitive.material].get(), nullptr);

                // Pass material parameters as push constants
                PushConstBlockMaterial csts{};
                csts.emissive_facotr = material.emissive_factor;
                csts.color_texture_set = material.base_color.is_valid() ? material.tex_coord_sets.base_color : -1;
                csts.normal_texture_set = material.normal.is_valid() ? material.tex_coord_sets.normal : -1;
                csts.occlusion_texture_set = material.occlusion.is_valid() ? material.tex_coord_sets.occlusion : -1;
                csts.emissive_texture_set = material.emissive.is_valid() ? material.tex_coord_sets.emissive : -1;
                csts.alpha_mask = static_cast<float>(material.alpha_mode == Material::AlphaMode::Mask);
                csts.alpha_mask_cutoff = material.alpha_cutoff;

                // TODO: glTF specs states that metallic roughness should be preferred, even if specular glosiness is present

                if (material.workflow == Material::PbrWorkflow::MetallicRoughness)
                {
                    csts.base_color_factor = material.base_color_factor;
                    csts.metallic_factor = material.metallic_factor;
                    csts.roughness_factor = material.roughness_factor;

                    csts.physical_descriptor_texture_set = material.metallic_roughness.is_valid() ? material.tex_coord_sets.metallic_roughness : -1;
                    csts.color_texture_set = material.base_color.is_valid() ? material.tex_coord_sets.base_color : -1;
                }

                if (material.workflow == Material::PbrWorkflow::SpecularGlossiness)
                {
                    csts.diffuse_factor = material.extension.diffuse_factor;
                    csts.specular_factor = glm::vec4(material.extension.specular_factor, 1.0f);

                    csts.physical_descriptor_texture_set = material.extension.specular_glosiness.is_valid() ? material.tex_coord_sets.specular_glosiness : -1;
                    csts.color_texture_set = material.extension.diffuse.is_valid() ? material.tex_coord_sets.base_color : -1;
                }

                cmd.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstBlockMaterial), &csts);

                cmd.drawIndexed(primitive.index_count, 1, primitive.first_index, static_cast<int32_t>(primitive.first_vertex), 0);
            }
        }

        for (const auto& child : node.children) {
            draw_rec(model, child, cmd, pipeline_layout, transforms_set, materials_set);
        }
    }

    void Model::draw(vk::CommandBuffer cmd, vk::PipelineLayout pipeline_layout, const MultipleDescriptorSet& transforms_set, const MultipleDescriptorSet& materials_set) const
    {
        for (auto& node: scene_nodes)
        {
            draw_rec(*this, node, cmd, pipeline_layout, transforms_set, materials_set);
        }
    }
}    // namespace my_app
