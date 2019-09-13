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

    void VoxelVisualization::init()
    {
        for (size_t i = 0; i < NUM_VIRTUAL_FRAME; i++)
        {
            auto& buffer = uniform_buffers[i];

            std::string name = "VoxelVisualization Uniform buffer ";
            name += std::to_string(i);
            buffer = Buffer{name, vulkan.allocator, sizeof(SceneUniform), vk::BufferUsageFlagBits::eUniformBuffer};
        }

        create_descriptors();
        update_descriptors();
        create_pipeline();
    }

    void VoxelVisualization::create_descriptors()
    {
        std::array<vk::DescriptorPoolSize, 1> pool_sizes;
        pool_sizes[0].type = vk::DescriptorType::eUniformBuffer;
        pool_sizes[0].descriptorCount = 2;


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
            bindings[0].stageFlags = vk::ShaderStageFlagBits::eGeometry;

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
        ImGui::SetNextWindowPos(ImVec2(200.f, 20.0f));
        ImGui::Begin("Uniform buffer", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        SceneUniform ubo{};
        ubo.view = glm::lookAt(
            camera.position,                   // origin of camera
            camera.position + camera.front,    // where to look
            camera.up);

        static float fov = 45.0f;
        ImGui::DragFloat("FOV", &fov, 1.0f, 20.f, 90.f);
        float aspect_ratio = static_cast<float>(renderer.get_swapchain().extent.width) / static_cast<float>(renderer.get_swapchain().extent.height);

        ubo.proj = glm::perspective(glm::radians(fov), aspect_ratio, 0.1f, 500.0f);

        // Vulkan clip space has inverted Y and half Z.
        ubo.clip = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f,
                             0.0f, -1.0f, 0.0f, 0.0f,
                             0.0f, 0.0f, 0.5f, 0.0f,
                             0.0f, 0.0f, 0.5f, 1.0f);

        ubo.cam_pos = glm::vec4(camera.position, 0.f);

        ImGui::Separator();

        static float light_dir[3] = { 1.0f, 1.0f, 1.0f };
        static bool animate_light = false;

        ImGui::SliderFloat3("Light direction", light_dir, -40.0f, 40.0f);
        ImGui::Checkbox("Rotate", &animate_light);
        if (animate_light)
        {
            light_dir[0] += 0.01f;
            if (light_dir[0] > 45.f)
                light_dir[0] = -45.f;
        }

        ubo.light_dir.x = light_dir[0];
        ubo.light_dir.y = light_dir[1];
        ubo.light_dir.z = light_dir[2];

        ImGui::Separator();

        static float ambient = 0.1f;
        ImGui::DragFloat("Ambient light", &ambient, 0.01f, 0.f, 1.f);
        ubo.ambient = ambient;

        ImGui::Separator();

        static size_t debug_view_input = 0;
        const char* input_items[] = { "Disabled", "Base color", "normal", "occlusion", "emissive", "physical 1", "physical 2" };
        tools::imgui_select("Debug inputs", input_items, ARRAY_SIZE(input_items), debug_view_input);
        ubo.debug_view_input = float(debug_view_input);

        ImGui::Separator();

        static size_t debug_view_equation = 0;
        const char* equation_items[] = { "Disabled", "Diff (l,n)", "F (l,h)", "G (l,v,h)", "D (h)", "Specular" };
        tools::imgui_select("Debug Equations", equation_items, ARRAY_SIZE(equation_items), debug_view_equation);
        ubo.debug_view_equation = float(debug_view_equation);

        ImGui::Separator();

        static float cube_scale = 0.5f;
        ImGui::DragFloat("Voxel cube scale", &cube_scale, 0.1f, 0.1f, 1.f);
        ubo.cube_scale = cube_scale;


        ImGui::End();

        void* uniform_data = uniform_buffers[frame_idx].map();
        memcpy(uniform_data, &ubo, sizeof(ubo));
    }

    void VoxelVisualization::create_pipeline()
    {
        auto bindings = Voxel::get_binding_description();
        auto attributes = Voxel::get_attribute_description();

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
        rast_i.cullMode = vk::CullModeFlagBits::eBack;
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

        std::array<vk::DescriptorSetLayout, 1> layouts = {
            scenes.layout.get()
        };

        vk::PipelineLayoutCreateInfo ci{};
        ci.pSetLayouts = layouts.data();
        ci.setLayoutCount = layouts.size();

        vk::PushConstantRange pcr{ vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstBlockMaterial) };
        ci.pushConstantRangeCount = 1;
        ci.pPushConstantRanges = &pcr;

        graphics_pipeline.layout = vulkan.device->createPipelineLayoutUnique(ci);

        auto vert_code = tools::readFile("build/shaders/shader.vert.spv");
        auto frag_code = tools::readFile("build/shaders/shader.frag.spv");
        auto geom_code = tools::readFile("build/shaders/voxel_debug.geom.spv");

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

    void VoxelVisualization::do_subpass(uint32_t frame_idx, vk::CommandBuffer cmd, const Buffer& voxels_buffer)
    {
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline.handle.get());
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphics_pipeline.layout.get(), 0, scenes.descriptors[frame_idx].get(), nullptr);
        cmd.bindVertexBuffers(0, voxels_buffer.get_buffer(), {0});
        cmd.draw(VOXEL_GRID_SIZE*VOXEL_GRID_SIZE*VOXEL_GRID_SIZE, 1, 0, 0);
    }
}
