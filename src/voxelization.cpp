#include "voxelization.hpp"

#include "tools.hpp"
#include "vulkan_context.hpp"
#include "renderer.hpp"
#include "model.hpp"

namespace my_app
{

    VoxelizationSubpass::VoxelizationSubpass(Renderer& _renderer, uint32_t _subpass)
        : renderer{_renderer}
        , vulkan{renderer.get_vulkan()}
        , subpass{_subpass}
    {}

    VoxelizationSubpass::~VoxelizationSubpass()
    {
        empty_image.free();
        index_buffer.free();
        vertex_buffer.free();
        voxels_texture.free();
        model.free(vulkan);

        for (size_t i = 0; i < model.meshes.size(); i++)
        {
            auto& buffer = mesh_buffers[i];
            buffer.free();
        }

        for (size_t i = 0; i < NUM_VIRTUAL_FRAME; i++)
        {
            auto buffer = debug_options[i];
            buffer.free();
        }

        vulkan.device->destroy(empty_info.sampler);
        vulkan.device->destroy(empty_info.imageView);
    }

    static void fill_mesh_uniform(vk::CommandBuffer cmd, const VulkanContext& vulkan, const Model& model, std::vector<Buffer>& mesh_buffers, const Node& node, std::vector<Buffer>& stagings)
    {
        if (node.mesh.is_valid())
        {
            auto& buffer = mesh_buffers[node.mesh.index];

            std::string name = "Mesh buffer ";
            name += std::to_string(node.mesh.index);
            buffer = Buffer{vulkan, sizeof(glm::mat4), vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst, name.data(),  VMA_MEMORY_USAGE_GPU_ONLY};

            auto matrix = node.get_matrix();

            VulkanContext::CopyDataToBufferParams i_params(buffer);
            i_params.data = &matrix;
            i_params.data_size = sizeof(matrix);
            i_params.generating_stages = vk::PipelineStageFlagBits::eTopOfPipe;
            i_params.new_buffer_access = vk::AccessFlagBits::eShaderRead;
            i_params.consuming_stages = vk::PipelineStageFlagBits::eVertexShader;

            stagings.push_back(vulkan.copy_data_to_buffer_cmd(cmd, i_params));
        }

        for (const auto& child: node.children) {
            fill_mesh_uniform(cmd, vulkan, model, mesh_buffers, child, stagings);
        }
    }

    void VoxelizationSubpass::init(const std::string& model_path)
    {
        model = Model(model_path, vulkan);

        auto cmd = vulkan.texture_command_buffer.get();

        cmd.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

        auto size = model.indices.size() * sizeof(uint32_t);
        index_buffer = Buffer(vulkan, size, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, "Voxelization Index buffer", VMA_MEMORY_USAGE_GPU_ONLY);

        VulkanContext::CopyDataToBufferParams i_params(index_buffer);
        i_params.data = model.indices.data();
        i_params.data_size = size;
        i_params.generating_stages = vk::PipelineStageFlagBits::eTopOfPipe;
        i_params.new_buffer_access = vk::AccessFlagBits::eIndexRead;
        i_params.consuming_stages = vk::PipelineStageFlagBits::eVertexInput;
        auto staging1 = vulkan.copy_data_to_buffer_cmd(cmd, i_params);

        size = model.vertices.size() * sizeof(Vertex);
        vertex_buffer = Buffer(vulkan, size, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, "Voxelization Vertex buffer", VMA_MEMORY_USAGE_GPU_ONLY);
        VulkanContext::CopyDataToBufferParams v_params(vertex_buffer);
        v_params.data = model.vertices.data();
        v_params.data_size = size;
        v_params.generating_stages = vk::PipelineStageFlagBits::eTopOfPipe;
        v_params.new_buffer_access = vk::AccessFlagBits::eVertexAttributeRead;
        v_params.consuming_stages = vk::PipelineStageFlagBits::eVertexInput;
        auto staging2 = vulkan.copy_data_to_buffer_cmd(cmd, v_params);

        mesh_buffers.resize(model.meshes.size());
        std::vector<Buffer> stagings;
        for (auto& node: model.scene_nodes) {
            fill_mesh_uniform(cmd, vulkan, model, mesh_buffers, node, stagings);
        }

        vk::ImageCreateInfo ci;
        ci.imageType = vk::ImageType::e3D;
        ci.format = vk::Format::eR8G8B8A8Unorm;
        ci.mipLevels = 1;
        ci.arrayLayers = 1;
        ci.samples = vk::SampleCountFlagBits::e1;
        ci.tiling = vk::ImageTiling::eOptimal;
        ci.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst;
        ci.sharingMode = vk::SharingMode::eExclusive;
        ci.initialLayout = vk::ImageLayout::eUndefined;
        ci.extent = vk::Extent3D{VOXEL_GRID_SIZE, VOXEL_GRID_SIZE, VOXEL_GRID_SIZE};

        voxels_texture = Image{vulkan, ci, "Voxels 3D texture"};
        vulkan.transition_layout_cmd(cmd, voxels_texture.get_image(), THSVS_ACCESS_NONE, THSVS_ACCESS_ANY_SHADER_READ_OTHER, voxels_texture.get_range(vk::ImageAspectFlagBits::eColor));

        cmd.end();
        vulkan.submit_and_wait_cmd(cmd);

        staging1.free();
        staging2.free();
        for (auto& staging: stagings)
            staging.free();

        for (size_t i = 0; i < NUM_VIRTUAL_FRAME; i++)
        {
            auto& buffer = debug_options[i];

            std::string name = "Voxelization debug options ";
            name += std::to_string(i);
            buffer = Buffer{vulkan, sizeof(VoxelizationOptions), vk::BufferUsageFlagBits::eUniformBuffer, name.c_str()};
        }

        create_empty();
        create_descriptors();
        update_descriptors();
        create_pipeline();
    }


    void VoxelizationSubpass::create_empty()
    {
        auto format = vk::Format::eA8B8G8R8UnormPack32;

        vk::ImageCreateInfo ci{};
        ci.imageType = vk::ImageType::e2D;
        ci.format = format;
        ci.extent.width = 1;
        ci.extent.height = 1;
        ci.extent.depth = 1;
        ci.mipLevels = 1;
        ci.arrayLayers = 1;
        ci.samples = vk::SampleCountFlagBits::e1;
        ci.initialLayout = vk::ImageLayout::eUndefined;
        ci.usage = vk::ImageUsageFlagBits::eSampled;
        ci.queueFamilyIndexCount = 0;
        ci.pQueueFamilyIndices = nullptr;
        ci.sharingMode = vk::SharingMode::eExclusive;
        ci.flags = {};
        empty_image = Image{vulkan, ci, "Empty image"};

        // Create the sampler for the texture
        TextureSampler texture_sampler;
        vk::SamplerCreateInfo sci{};
        sci.magFilter = texture_sampler.mag_filter;
        sci.minFilter = texture_sampler.min_filter;
        sci.mipmapMode = vk::SamplerMipmapMode::eLinear;
        sci.addressModeU = texture_sampler.address_mode_u;
        sci.addressModeV = texture_sampler.address_mode_v;
        sci.addressModeW = texture_sampler.address_mode_w;
        sci.compareOp = vk::CompareOp::eNever;
        sci.borderColor = vk::BorderColor::eFloatOpaqueWhite;
        sci.maxAnisotropy = 1.0;
        sci.anisotropyEnable = VK_FALSE;
        sci.maxLod = 1.0f;
        empty_info.sampler = vulkan.device->createSampler(sci);

        // Create the image view holding the texture
        vk::ImageSubresourceRange subresource_range;
        subresource_range.aspectMask = vk::ImageAspectFlagBits::eColor;
        subresource_range.baseMipLevel = 0;
        subresource_range.levelCount = 1;
        subresource_range.baseArrayLayer = 0;
        subresource_range.layerCount = 1;

        vk::ImageViewCreateInfo vci{};
        vci.flags = {};
        vci.image = empty_image.get_image();
        vci.format = format;
        vci.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
        vci.subresourceRange = subresource_range;
        vci.viewType = vk::ImageViewType::e2D;
        empty_info.imageView = vulkan.device->createImageView(vci);

        vulkan.transition_layout(empty_image.get_image(), THSVS_ACCESS_NONE, THSVS_ACCESS_ANY_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER, subresource_range);
        empty_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    }

    void VoxelizationSubpass::create_descriptors()
    {
        std::array<vk::DescriptorPoolSize, 3> pool_sizes;

        // Mesh transforms
        pool_sizes[0].type = vk::DescriptorType::eUniformBuffer;
        pool_sizes[0].descriptorCount = 1024;

        // Material textures, 5 textures per material
        pool_sizes[1].type = vk::DescriptorType::eCombinedImageSampler;
        pool_sizes[1].descriptorCount = 1024;

        // Voxels buffer
        pool_sizes[2].type = vk::DescriptorType::eStorageBuffer;
        pool_sizes[2].descriptorCount = 1;

        vk::DescriptorPoolCreateInfo dpci{};
        dpci.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
        dpci.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        dpci.pPoolSizes = pool_sizes.data();
        dpci.maxSets = 4096;
        desc_pool = vulkan.device->createDescriptorPoolUnique(dpci);

        // Descriptor set 0: Voxels
        {
            std::vector<vk::DescriptorSetLayoutBinding> bindings;
            bindings.resize(1);

            bindings[0].binding = 0;
            bindings[0].descriptorType = vk::DescriptorType::eStorageImage;
            bindings[0].descriptorCount = 1;
            bindings[0].stageFlags = vk::ShaderStageFlagBits::eFragment;

            voxels.layout = vulkan.create_descriptor_layout(bindings);

            vk::DescriptorSetAllocateInfo dsai;
            dsai.descriptorPool = desc_pool.get();
            dsai.pSetLayouts = &voxels.layout.get();
            dsai.descriptorSetCount = 1;
            voxels.descriptor = std::move(vulkan.device->allocateDescriptorSetsUnique(dsai)[0]);
        }

        // Descriptor set 1: Per frame descriptor (debug options)
        {
            std::vector<vk::DescriptorSetLayoutBinding> bindings;
            bindings.resize(1);

            bindings[0].binding = 0;
            bindings[0].descriptorType = vk::DescriptorType::eUniformBuffer;
            bindings[0].descriptorCount = 1;
            bindings[0].stageFlags = vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment;

            debug_voxel.layout = vulkan.create_descriptor_layout(bindings);

            std::vector<vk::DescriptorSetLayout> layouts{ NUM_VIRTUAL_FRAME, debug_voxel.layout.get() };

            vk::DescriptorSetAllocateInfo dsai{};
            dsai.descriptorPool = desc_pool.get();
            dsai.pSetLayouts = layouts.data();
            dsai.descriptorSetCount = static_cast<uint32_t>(layouts.size());

            debug_voxel.descriptors = vulkan.device->allocateDescriptorSetsUnique(dsai);
        }


        // Descriptor set 2: Nodes uniform (local transforms of each mesh)
        {
            std::vector<vk::DescriptorSetLayoutBinding> bindings;
            bindings.resize(1);

            bindings[0].binding = 0;
            bindings[0].descriptorType = vk::DescriptorType::eUniformBuffer;
            bindings[0].descriptorCount = 1;
            bindings[0].stageFlags = vk::ShaderStageFlagBits::eVertex;

            transforms.layout = vulkan.create_descriptor_layout(bindings);

            std::vector<vk::DescriptorSetLayout> layouts{ model.meshes.size(), transforms.layout.get() };

            vk::DescriptorSetAllocateInfo dsai{};
            dsai.descriptorPool = desc_pool.get();
            dsai.pSetLayouts = layouts.data();
            dsai.descriptorSetCount = static_cast<uint32_t>(layouts.size());
            transforms.descriptors = vulkan.device->allocateDescriptorSetsUnique(dsai);
        }

        // Descriptor set 3: Materials
        {
            std::vector<vk::DescriptorSetLayoutBinding> bindings;
            bindings.resize(5);

            for (uint32_t i = 0; i < 5; i++)
            {
                bindings[i].binding = i;
                bindings[i].descriptorType = vk::DescriptorType::eCombinedImageSampler;
                bindings[i].descriptorCount = 1;
                bindings[i].stageFlags = vk::ShaderStageFlagBits::eFragment;
            }

            materials.layout = vulkan.create_descriptor_layout(bindings);

            std::vector<vk::DescriptorSetLayout> layouts{ model.materials.size(), materials.layout.get() };

            vk::DescriptorSetAllocateInfo dsai{};
            dsai.descriptorPool = desc_pool.get();
            dsai.pSetLayouts = layouts.data();
            dsai.descriptorSetCount = static_cast<uint32_t>(layouts.size());
            materials.descriptors = vulkan.device->allocateDescriptorSetsUnique(dsai);

        }
    }

    void VoxelizationSubpass::update_descriptors()
    {
        {
            vk::DescriptorImageInfo ii;
            ii.sampler = voxels_texture.get_default_sampler();
            ii.imageLayout = vk::ImageLayout::eGeneral;
            ii.imageView = voxels_texture.get_default_view();

            std::array<vk::WriteDescriptorSet, 1> writes;
            writes[0].descriptorType = vk::DescriptorType::eStorageImage;
            writes[0].descriptorCount = 1;
            writes[0].dstSet = voxels.descriptor.get();
            writes[0].dstBinding = 0;
            writes[0].pImageInfo = &ii;

            vulkan.device->updateDescriptorSets(writes, nullptr);
        }
        {
            std::vector<vk::WriteDescriptorSet> writes;

            for (size_t i = 0; i < NUM_VIRTUAL_FRAME; i++)
            {
                auto& buffer = debug_options[i];
                auto bi = buffer.get_desc_info();

                vk::WriteDescriptorSet write{};
                write.descriptorType = vk::DescriptorType::eUniformBuffer;
                write.descriptorCount = 1;
                write.dstSet = debug_voxel.descriptors[i].get();
                write.dstBinding = 0;
                write.pBufferInfo = &bi;
                writes.push_back(std::move(write));
            }

            vulkan.device->updateDescriptorSets(writes, nullptr);
        }

        for (uint32_t mesh_i = 0; mesh_i < model.meshes.size(); mesh_i++)
        {
            auto bi = mesh_buffers[mesh_i].get_desc_info();

            std::vector<vk::WriteDescriptorSet> writes;
            vk::WriteDescriptorSet write{};
            write.descriptorType = vk::DescriptorType::eUniformBuffer;
            write.descriptorCount = 1;
            write.dstSet = transforms.descriptors[mesh_i].get();
            write.dstBinding = 0;
            write.pBufferInfo = &bi;
            writes.push_back(std::move(write));
            vulkan.device->updateDescriptorSets(writes, nullptr);
        }

        // Per-Material descriptor sets
        for (uint32_t material_i = 0; material_i < model.materials.size(); material_i++)
        {
            auto& material = model.materials[material_i];

            // Informations for each texture
            std::vector<vk::DescriptorImageInfo> image_descriptors = {
                empty_info,
                empty_info,
                material.normal.is_valid() ? model.textures[material.normal.index].desc_info : empty_info,
                material.occlusion.is_valid() ? model.textures[material.occlusion.index].desc_info : empty_info,
                material.emissive.is_valid() ? model.textures[material.emissive.index].desc_info : empty_info
            };

            // TODO: glTF specs states that metallic roughness should be preferred, even if specular glosiness is present
            if (material.workflow == Material::PbrWorkflow::MetallicRoughness)
            {
                if (material.base_color.is_valid())
                    image_descriptors[0] = model.textures[material.base_color.index].desc_info;
                if (material.metallic_roughness.is_valid())
                    image_descriptors[1] = model.textures[material.metallic_roughness.index].desc_info;
            }
            else
            {
                if (material.extension.diffuse.is_valid())
                    image_descriptors[0] = model.textures[material.extension.diffuse.index].desc_info;
                if (material.extension.specular_glosiness.is_valid())
                    image_descriptors[1] = model.textures[material.extension.specular_glosiness.index].desc_info;
            }

            std::vector<vk::WriteDescriptorSet> writes;
            // Fill then descriptor set with a binding for each texture
            for (uint32_t i = 0; i < image_descriptors.size(); i++)
            {
                vk::WriteDescriptorSet write{};
                write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
                write.descriptorCount = 1;
                write.dstSet = materials.descriptors[material_i].get();
                write.dstBinding = i;
                write.pImageInfo = &image_descriptors[i];
                writes.push_back(std::move(write));
            }
            vulkan.device->updateDescriptorSets(writes, nullptr);
        }

    }

    void VoxelizationSubpass::update_uniform_buffer(uint32_t frame_idx, Camera& camera)
    {
        VoxelizationOptions options;

        ImGui::SetNextWindowPos(ImVec2(600.f, 20.0f));
        ImGui::Begin("Voxelization options", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Separator();
        static float voxel_size = 1.f;
        ImGui::DragFloat("Voxel size", &voxel_size, 0.1f, 0.1f, 2.f);
        options.size = voxel_size;

        ImGui::Separator();

        static int voxel_res = 256;
        ImGui::DragInt("Voxel res", &voxel_res, 128.f, 128, 256);
        options.res = static_cast<uint32_t>(voxel_res);

        static float center[3] = { 0.0f, 0.0f, 0.0f };
        ImGui::SliderFloat3("Voxel center", center, -40.0f, 40.0f);
        center[0] = camera.position.x;
        center[1] = camera.position.y;
        center[2] = camera.position.z;
        options.center.x = center[0];
        options.center.y = center[1];
        options.center.z = center[2];


        ImGui::Separator();

        void* uniform_data = debug_options[frame_idx].map();
        memcpy(uniform_data, &options, sizeof(options));

        ImGui::End();
    }

    void VoxelizationSubpass::create_pipeline()
    {
        auto bindings = Vertex::get_binding_description();
        auto attributes = Vertex::get_attribute_description();

        vk::PipelineVertexInputStateCreateInfo vert_i{};
        vert_i.flags = vk::PipelineVertexInputStateCreateFlags(0);
        vert_i.vertexBindingDescriptionCount = bindings.size();
        vert_i.pVertexBindingDescriptions = bindings.data();
        vert_i.vertexAttributeDescriptionCount = attributes.size();
        vert_i.pVertexAttributeDescriptions = attributes.data();

        vk::PipelineInputAssemblyStateCreateInfo asm_i{};
        asm_i.flags = vk::PipelineInputAssemblyStateCreateFlags(0);
        asm_i.primitiveRestartEnable = VK_FALSE;
        asm_i.topology = vk::PrimitiveTopology::eTriangleList;

        vk::PipelineRasterizationStateCreateInfo rast_i{};
        rast_i.flags = vk::PipelineRasterizationStateCreateFlags(0);
        rast_i.polygonMode = vk::PolygonMode::eFill;
        rast_i.cullMode = vk::CullModeFlagBits::eNone;
        rast_i.frontFace = vk::FrontFace::eCounterClockwise;
        rast_i.depthClampEnable = VK_FALSE;
        rast_i.rasterizerDiscardEnable = VK_FALSE;
        rast_i.depthBiasEnable = VK_FALSE;
        rast_i.depthBiasConstantFactor = 0;
        rast_i.depthBiasClamp = 0;
        rast_i.depthBiasSlopeFactor = 0;
        rast_i.lineWidth = 1.0f;

        vk::PipelineColorBlendStateCreateInfo colorblend_i{};
        colorblend_i.flags = vk::PipelineColorBlendStateCreateFlags(0);
        colorblend_i.attachmentCount = 0;
        colorblend_i.pAttachments = nullptr;
        colorblend_i.logicOpEnable = VK_FALSE;
        colorblend_i.logicOp = vk::LogicOp::eCopy;
        colorblend_i.blendConstants[0] = 0.0f;
        colorblend_i.blendConstants[1] = 0.0f;
        colorblend_i.blendConstants[2] = 0.0f;
        colorblend_i.blendConstants[3] = 0.0f;

        vk::PipelineViewportStateCreateInfo vp_i{};
        vp_i.flags = vk::PipelineViewportStateCreateFlags();
        vp_i.viewportCount = 1;
        vp_i.scissorCount = 1;
        vp_i.pScissors = nullptr;
        vp_i.pViewports = nullptr;

        vk::PipelineDepthStencilStateCreateInfo ds_i{};
        ds_i.flags = vk::PipelineDepthStencilStateCreateFlags();
        ds_i.depthTestEnable = VK_FALSE;
        ds_i.depthWriteEnable = VK_FALSE;
        ds_i.depthCompareOp = vk::CompareOp::eLessOrEqual;
        ds_i.depthBoundsTestEnable = VK_FALSE;
        ds_i.minDepthBounds = 0.0f;
        ds_i.maxDepthBounds = 0.0f;
        ds_i.stencilTestEnable = VK_FALSE;
        ds_i.back.failOp = vk::StencilOp::eKeep;
        ds_i.back.passOp = vk::StencilOp::eKeep;
        ds_i.back.compareOp = vk::CompareOp::eAlways;
        ds_i.back.compareMask = 0;
        ds_i.back.reference = 0;
        ds_i.back.depthFailOp = vk::StencilOp::eKeep;
        ds_i.back.writeMask = 0;
        ds_i.front = ds_i.back;

        vk::PipelineMultisampleStateCreateInfo ms_i{};
        ms_i.flags = vk::PipelineMultisampleStateCreateFlags();
        ms_i.pSampleMask = nullptr;
        ms_i.rasterizationSamples = MSAA_SAMPLES;
        ms_i.sampleShadingEnable = VK_FALSE;
        ms_i.alphaToCoverageEnable = VK_FALSE;
        ms_i.alphaToOneEnable = VK_FALSE;
        ms_i.minSampleShading = .2f;

        std::array<vk::DescriptorSetLayout, 4> layouts = {
            voxels.layout.get(), debug_voxel.layout.get(), transforms.layout.get(), materials.layout.get()
        };

        vk::PipelineLayoutCreateInfo ci{};
        ci.pSetLayouts = layouts.data();
        ci.setLayoutCount = layouts.size();

        vk::PushConstantRange pcr{ vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstBlockMaterial) };
        ci.pushConstantRangeCount = 1;
        ci.pPushConstantRanges = &pcr;

        graphics_pipeline.layout = vulkan.device->createPipelineLayoutUnique(ci);

        auto vert_code = tools::readFile("build/shaders/voxelization.vert.spv");
        auto frag_code = tools::readFile("build/shaders/voxelization.frag.spv");
        auto geom_code = tools::readFile("build/shaders/voxelization.geom.spv");

        auto vert_module = vulkan.create_shader_module(vert_code);
        auto frag_module = vulkan.create_shader_module(frag_code);
        auto geom_module = vulkan.create_shader_module(geom_code);

        std::array<vk::PipelineShaderStageCreateInfo, 3> shader_stages;
        shader_stages[0].stage = vk::ShaderStageFlagBits::eVertex;
        shader_stages[0].module = vert_module.get();
        shader_stages[0].pName = "main";
        shader_stages[1].stage = vk::ShaderStageFlagBits::eFragment;
        shader_stages[1].module = frag_module.get();
        shader_stages[1].pName = "main";
        shader_stages[2].stage = vk::ShaderStageFlagBits::eGeometry;
        shader_stages[2].module = geom_module.get();
        shader_stages[2].pName = "main";

        std::vector<vk::DynamicState> dynamic_states =
        {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor,
        };

        vk::PipelineDynamicStateCreateInfo dyn_i{ {}, static_cast<uint32_t>(dynamic_states.size()), dynamic_states.data() };

        vk::GraphicsPipelineCreateInfo pipe_i{};
        pipe_i.layout = graphics_pipeline.layout.get();
        pipe_i.basePipelineHandle = nullptr;
        pipe_i.basePipelineIndex = 0;
        pipe_i.pVertexInputState = &vert_i;
        pipe_i.pInputAssemblyState = &asm_i;
        pipe_i.pRasterizationState = &rast_i;
        pipe_i.pColorBlendState = &colorblend_i;
        pipe_i.pTessellationState = nullptr;
        pipe_i.pMultisampleState = &ms_i;
        pipe_i.pDynamicState = &dyn_i;
        pipe_i.pViewportState = &vp_i;
        pipe_i.pDepthStencilState = &ds_i;
        pipe_i.pStages = shader_stages.data();
        pipe_i.stageCount = static_cast<uint32_t>(shader_stages.size());
        pipe_i.renderPass = renderer.get_render_pass();
        pipe_i.subpass = subpass;

        graphics_pipeline.cache = vulkan.device->createPipelineCacheUnique({});
        graphics_pipeline.handle = vulkan.device->createGraphicsPipelineUnique(graphics_pipeline.cache.get(), pipe_i);
    }

    void VoxelizationSubpass::before_subpass(uint32_t, vk::CommandBuffer cmd)
    {
        vk::ClearColorValue clear;
        cmd.clearColorImage(voxels_texture.get_image(), vk::ImageLayout::eGeneral, clear, voxels_texture.get_range(vk::ImageAspectFlagBits::eColor));
    }

    void VoxelizationSubpass::do_subpass(uint32_t frame_idx, vk::CommandBuffer cmd)
    {
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline.handle.get());
        std::vector<vk::DescriptorSet> sets = {voxels.descriptor.get(), debug_voxel.descriptors[frame_idx].get()};
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphics_pipeline.layout.get(), 0, sets, nullptr);
        cmd.bindVertexBuffers(0, vertex_buffer.get_buffer(), {0});
        cmd.bindIndexBuffer(index_buffer.get_buffer(), 0, vk::IndexType::eUint32);

        model.draw(cmd, graphics_pipeline.layout.get(), transforms, materials);
    }
}
