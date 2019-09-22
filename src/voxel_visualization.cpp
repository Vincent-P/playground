#include "voxel_visualization.hpp"

#include "tools.hpp"
#include "vulkan_context.hpp"
#include "renderer.hpp"

namespace my_app
{

    VoxelVisualization::VoxelVisualization(Renderer& _renderer, uint32_t _subpass)
        : renderer{_renderer}
        , vulkan{renderer.get_vulkan()}
        , subpass{_subpass}
    {}

    VoxelVisualization::~VoxelVisualization()
    {
        for (size_t i = 0; i < NUM_VIRTUAL_FRAME; i++)
        {
            auto buffer = uniform_buffers[i];
            buffer.free();
        }
    }

    void VoxelVisualization::init(vk::DescriptorSetLayout _voxels_texture_layout)
    {
        for (size_t i = 0; i < NUM_VIRTUAL_FRAME; i++)
        {
            auto& buffer = uniform_buffers[i];

            std::string name = "VoxelVisualization Uniform buffer ";
            name += std::to_string(i);
            buffer = Buffer{vulkan, sizeof(SceneUniform), vk::BufferUsageFlagBits::eUniformBuffer, name.data()};
        }

        create_descriptors();
        update_descriptors();
        create_pipeline(_voxels_texture_layout);
    }

    void VoxelVisualization::create_descriptors()
    {
        std::array<vk::DescriptorPoolSize, 1> pool_sizes;
        pool_sizes[0].type = vk::DescriptorType::eUniformBuffer;
        pool_sizes[0].descriptorCount = NUM_VIRTUAL_FRAME;


        vk::DescriptorPoolCreateInfo dpci{};
        dpci.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
        dpci.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        dpci.pPoolSizes = pool_sizes.data();
        dpci.maxSets = 2;
        desc_pool = vulkan.device->createDescriptorPoolUnique(dpci);

        // Descriptor set 0: Scene/Camera informations (MVP)
        {
            std::vector<vk::DescriptorSetLayoutBinding> bindings;
            bindings.resize(1);

            bindings[0].binding = 0;
            bindings[0].descriptorType = vk::DescriptorType::eUniformBuffer;
            bindings[0].descriptorCount = 1;
            bindings[0].stageFlags = vk::ShaderStageFlagBits::eFragment;

            scenes.layout = vulkan.create_descriptor_layout(bindings);

            std::vector<vk::DescriptorSetLayout> layouts{ NUM_VIRTUAL_FRAME, scenes.layout.get() };

            vk::DescriptorSetAllocateInfo dsai{};
            dsai.descriptorPool = desc_pool.get();
            dsai.pSetLayouts = layouts.data();
            dsai.descriptorSetCount = static_cast<uint32_t>(layouts.size());

            scenes.descriptors = vulkan.device->allocateDescriptorSetsUnique(dsai);
        }

    }

    void VoxelVisualization::update_descriptors()
    {
        std::vector<vk::WriteDescriptorSet> writes;

        for (size_t i = 0; i < NUM_VIRTUAL_FRAME; i++)
        {
            auto& buffer = uniform_buffers[i];
            auto bi = buffer.get_desc_info();

            vk::WriteDescriptorSet write{};
            write.descriptorType = vk::DescriptorType::eUniformBuffer;
            write.descriptorCount = 1;
            write.dstSet = scenes.descriptors[i].get();
            write.dstBinding = 0;
            write.pBufferInfo = &bi;
            writes.push_back(std::move(write));
        }

        vulkan.device->updateDescriptorSets(writes, nullptr);
    }

    void VoxelVisualization::update_uniform_buffer(uint32_t frame_idx, Camera& camera)
    {
        SceneUniform ubo = {};
        ubo.cam_pos = glm::vec4(camera.position, 0.f);
        ubo.cam_front = glm::vec4(camera.front, 0.f);
        ubo.cam_up = glm::vec4(camera.up, 0.f);

        void* uniform_data = uniform_buffers[frame_idx].map();
        memcpy(uniform_data, &ubo, sizeof(ubo));
    }

    void VoxelVisualization::create_pipeline(vk::DescriptorSetLayout _voxels_texture_layout)
    {
        vk::PipelineVertexInputStateCreateInfo vert_i{};
        vert_i.flags = vk::PipelineVertexInputStateCreateFlags(0);
        vert_i.vertexBindingDescriptionCount = 0;
        vert_i.pVertexBindingDescriptions = nullptr;
        vert_i.vertexAttributeDescriptionCount = 0;
        vert_i.pVertexAttributeDescriptions = nullptr;

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

        std::array<vk::PipelineColorBlendAttachmentState, 1> att_states;
        att_states[0].colorWriteMask = { vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA };
        att_states[0].blendEnable = VK_TRUE;
        att_states[0].srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
        att_states[0].dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
        att_states[0].colorBlendOp = vk::BlendOp::eAdd;
        att_states[0].srcAlphaBlendFactor = vk::BlendFactor::eOne;
        att_states[0].dstAlphaBlendFactor = vk::BlendFactor::eZero;
        att_states[0].alphaBlendOp = vk::BlendOp::eAdd;

        vk::PipelineColorBlendStateCreateInfo colorblend_i{};
        colorblend_i.flags = vk::PipelineColorBlendStateCreateFlags(0);
        colorblend_i.attachmentCount = static_cast<uint32_t>(att_states.size());
        colorblend_i.pAttachments = att_states.data();
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
        ds_i.depthTestEnable = VK_TRUE;
        ds_i.depthWriteEnable = VK_TRUE;
        ds_i.depthCompareOp = vk::CompareOp::eLess;
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

        std::array<vk::DescriptorSetLayout, 2> layouts = {
            _voxels_texture_layout,
            scenes.layout.get()
        };

        vk::PipelineLayoutCreateInfo ci{};
        ci.pSetLayouts = layouts.data();
        ci.setLayoutCount = layouts.size();

        vk::PushConstantRange pcr{ vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstBlockMaterial) };
        ci.pushConstantRangeCount = 1;
        ci.pPushConstantRanges = &pcr;

        graphics_pipeline.layout = vulkan.device->createPipelineLayoutUnique(ci);

        auto vert_code = tools::readFile("build/shaders/voxel_visualization.vert.spv");
        auto frag_code = tools::readFile("build/shaders/voxel_visualization.frag.spv");

        auto vert_module = vulkan.create_shader_module(vert_code);
        auto frag_module = vulkan.create_shader_module(frag_code);

        std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages;
        shader_stages[0].stage = vk::ShaderStageFlagBits::eVertex;
        shader_stages[0].module = vert_module.get();
        shader_stages[0].pName = "main";
        shader_stages[1].stage = vk::ShaderStageFlagBits::eFragment;
        shader_stages[1].module = frag_module.get();
        shader_stages[1].pName = "main";

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

    void VoxelVisualization::do_subpass(uint32_t frame_idx, vk::CommandBuffer cmd)
    {
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline.handle.get());
        // hopeofully it will keep the descriptor 0 from the voxelization pipeline
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphics_pipeline.layout.get(), 1, scenes.descriptors[frame_idx].get(), nullptr);
        cmd.draw(6, 1, 0, 0);
    }
}
