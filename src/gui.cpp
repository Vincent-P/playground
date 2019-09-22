#include "gui.hpp"

#include <imgui.h>
#include <iostream>
#include <thsvs/thsvs_simpler_vulkan_synchronization.h>

#include "renderer.hpp"
#include "timer.hpp"
#include "tools.hpp"

namespace my_app
{
    GUI::GUI(Renderer& _renderer, uint32_t _subpass)
        : renderer{ _renderer }
        , vulkan{renderer.get_vulkan()}
        , subpass{_subpass}
    {}

    GUI::~GUI()
    {
        vulkan.device->destroy(texture_desc_info.imageView);
        vulkan.device->destroy(texture_desc_info.sampler);
        texture.free();

        for (auto& resource : resources)
        {
            resource.vertex_buffer.free();
            resource.index_buffer.free();
        }

        if (ImGui::GetCurrentContext())
            ImGui::DestroyContext();
    }

    void GUI::init()
    {
        ImGui::CreateContext();
        auto& style = ImGui::GetStyle();
        style.FrameRounding = 0.f;
        style.GrabRounding = 0.f;
        style.WindowRounding = 0.f;
        style.ScrollbarRounding = 0.f;
        style.GrabRounding = 0.f;
        style.TabRounding = 0.f;


        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize.x = float(renderer.get_swapchain().extent.width);
        io.DisplaySize.y = float(renderer.get_swapchain().extent.height);

        resources.resize(NUM_VIRTUAL_FRAME);

        create_texture();
        create_descriptors();
        create_pipeline_layout();
        create_graphics_pipeline();
    }

    void GUI::start_frame(const TimerData& timer) const
    {
        ImGuiIO& io = ImGui::GetIO();
        io.DeltaTime = timer.get_delta_time();
        io.Framerate = timer.get_average_fps();

        io.DisplaySize.x = float(renderer.get_swapchain().extent.width);
        io.DisplaySize.y = float(renderer.get_swapchain().extent.height);

        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 120.0f, 20.0f));
        ImGui::SetNextWindowSize(ImVec2(100.0f, 100.0));
        ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);

        static bool show_fps = false;

        if (ImGui::RadioButton("FPS", show_fps))
        {
            show_fps = true;
        }

        ImGui::SameLine();

        if (ImGui::RadioButton("ms", !show_fps))
        {
            show_fps = false;
        }

        if (show_fps)
        {
            ImGui::SetCursorPosX(20.0f);
            ImGui::Text("%7.1f", double(timer.get_average_fps()));

            auto& histogram = timer.get_fps_histogram();
            ImGui::PlotHistogram("", histogram.data(), static_cast<int>(histogram.size()), 0, nullptr, 0.0f, FLT_MAX, ImVec2(85.0f, 30.0f));
        }
        else
        {
            ImGui::SetCursorPosX(20.0f);
            ImGui::Text("%9.3f", double(timer.get_average_delta_time()));

            auto& histogram = timer.get_delta_time_histogram();
            ImGui::PlotHistogram("", histogram.data(), static_cast<int>(histogram.size()), 0, nullptr, 0.0f, FLT_MAX, ImVec2(85.0f, 30.0f));
        }

        ImGui::End();
    }

    void GUI::do_subpass(uint32_t resource_index, vk::CommandBuffer cmd)
    {
        // Bind pipeline and set pipeline state
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.get());
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout.get(), 0, { desc_set.get() }, {});

        vk::Viewport viewport(
            0.0f,                            // float                                  x
            0.0f,                            // float                                  y
            ImGui::GetIO().DisplaySize.x,    // float                                  width
            ImGui::GetIO().DisplaySize.y,    // float                                  height
            0.0f,                            // float                                  minDepth
            1.0f                             // float                                  maxDepth
        );
        cmd.setViewport(0, { viewport });

        // Draw
        draw_frame_data(resource_index, cmd);
    }


    void GUI::draw_frame_data(uint32_t resource_index, vk::CommandBuffer cmd)
    {
        ImGui::Render();
        auto& resource = resources[resource_index];

        ImDrawData* data = ImGui::GetDrawData();
        if (!data || data->TotalVtxCount == 0)
            return;

        // Check that there is enough space for the vertices
        uint32_t vertex_buffer_size = sizeof(ImDrawVert) * static_cast<uint32_t>(data->TotalVtxCount);
        if (resource.vertex_buffer.get_size() < vertex_buffer_size)
        {
            resource.vertex_buffer.free();
            resource.vertex_buffer = Buffer{vulkan, vertex_buffer_size, vk::BufferUsageFlagBits::eVertexBuffer,  "GUI Vertex buffer"};
        }

        // Check that there is enough space for the indices
        uint32_t index_buffer_size = sizeof(ImDrawIdx) * static_cast<uint32_t>(data->TotalIdxCount);
        if (resource.index_buffer.get_size() < index_buffer_size)
        {
            resource.index_buffer.free();
            resource.index_buffer = Buffer{vulkan, index_buffer_size, vk::BufferUsageFlagBits::eIndexBuffer,  "GUI Index buffer"};
        }

        // Upload vertex and index data
        ImDrawVert* vertices = reinterpret_cast<ImDrawVert*>(resource.vertex_buffer.map());
        ImDrawIdx* indices = reinterpret_cast<ImDrawIdx*>(resource.index_buffer.map());

        for (int i = 0; i < data->CmdListsCount; i++)
        {
            const ImDrawList* cmd_list = data->CmdLists[i];

            std::memcpy(vertices, cmd_list->VtxBuffer.Data, sizeof(ImDrawVert) * size_t(cmd_list->VtxBuffer.Size));
            std::memcpy(indices, cmd_list->IdxBuffer.Data, sizeof(ImDrawIdx) * size_t(cmd_list->IdxBuffer.Size));

            vertices += cmd_list->VtxBuffer.Size;
            indices += cmd_list->IdxBuffer.Size;
        }

        resource.vertex_buffer.flush();
        resource.index_buffer.flush();

        // Bind vertex and index buffers
        cmd.bindVertexBuffers(0, { resource.vertex_buffer.get_buffer() }, { 0 });
        cmd.bindIndexBuffer(resource.index_buffer.get_buffer(), 0, vk::IndexType::eUint16);

        // Setup scale and translation:
        std::vector<float> scale_and_translation = {
            2.0f / ImGui::GetIO().DisplaySize.x,    // X scale
            2.0f / ImGui::GetIO().DisplaySize.y,    // Y scale
            -1.0f,                                  // X translation
            -1.0f                                   // Y translation
        };

        cmd.pushConstants(pipeline_layout.get(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(float) * static_cast<uint32_t>(scale_and_translation.size()), scale_and_translation.data());

        // Render GUI
        int32_t vertex_offset = 0;
        uint32_t index_offset = 0;
        for (int list = 0; list < data->CmdListsCount; list++)
        {
            const ImDrawList* cmd_list = data->CmdLists[list];

            for (int command_index = 0; command_index < cmd_list->CmdBuffer.Size; command_index++)
            {
                const ImDrawCmd* draw_command = &cmd_list->CmdBuffer[command_index];

                vk::Rect2D scissor(
                    vk::Offset2D(
                        int32_t(draw_command->ClipRect.x) > 0 ? int32_t(draw_command->ClipRect.x) : 0,
                        int32_t(draw_command->ClipRect.y) > 0 ? int32_t(draw_command->ClipRect.y) : 0),
                    vk::Extent2D(
                        uint32_t(draw_command->ClipRect.z - draw_command->ClipRect.x),
                        uint32_t(draw_command->ClipRect.w - draw_command->ClipRect.y)));

                cmd.setScissor(0, { scissor });
                cmd.drawIndexed(draw_command->ElemCount, 1, index_offset, vertex_offset, 0);

                index_offset += draw_command->ElemCount;
            }
            vertex_offset += cmd_list->VtxBuffer.Size;
        }
    }


    void GUI::create_texture()
    {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t data_size = 0;
        unsigned char* pixels;

        // Get image data
        int w = 0, h = 0;
        ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
        width = static_cast<uint32_t>(w);
        height = static_cast<uint32_t>(h);
        data_size = width * height * 4;

        // Create image and sampler
        vk::Format format = vk::Format::eR8G8B8A8Unorm;

        vk::ImageCreateInfo ci{};
        ci.flags = {};
        ci.imageType = vk::ImageType::e2D;
        ci.format = format;
        ci.extent.width = width;
        ci.extent.height = height;
        ci.extent.depth = 1;
        ci.mipLevels = 1;
        ci.arrayLayers = 1;
        ci.samples = vk::SampleCountFlagBits::e1;
        ci.initialLayout = vk::ImageLayout::eUndefined;
        ci.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
        ci.queueFamilyIndexCount = 0;
        ci.pQueueFamilyIndices = nullptr;
        ci.sharingMode = vk::SharingMode::eExclusive;

        texture = Image{vulkan, ci, "GUI Texture"};


        vk::ImageViewCreateInfo vci{};
        vci.flags = {};
        vci.image = texture.get_image();
        vci.format = format;
        vci.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
        vci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        vci.subresourceRange.baseMipLevel = 0;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.baseArrayLayer = 0;
        vci.subresourceRange.layerCount = 1;
        vci.viewType = vk::ImageViewType::e2D;

        texture_desc_info.imageView = vulkan.device->createImageView(vci);

        vk::SamplerCreateInfo sci{};
        sci.magFilter = vk::Filter::eNearest;
        sci.minFilter = vk::Filter::eLinear;
        sci.mipmapMode = vk::SamplerMipmapMode::eLinear;
        sci.addressModeU = vk::SamplerAddressMode::eRepeat;
        sci.addressModeV = vk::SamplerAddressMode::eRepeat;
        sci.addressModeW = vk::SamplerAddressMode::eRepeat;
        sci.compareOp = vk::CompareOp::eNever;
        sci.borderColor = vk::BorderColor::eFloatOpaqueWhite;
        sci.minLod = 0;
        sci.maxLod = 0;
        sci.maxAnisotropy = 8.0f;
        sci.anisotropyEnable = VK_TRUE;
        texture_desc_info.sampler = vulkan.device->createSampler(sci);

        // Copy data to image's memory
        vk::ImageSubresourceRange isr{};
        isr.aspectMask = vk::ImageAspectFlagBits::eColor;
        isr.baseMipLevel = 0;
        isr.levelCount = 1;
        isr.baseArrayLayer = 0;
        isr.layerCount = 1;

        VulkanContext::CopyDataToImageParams params(texture, isr);
        params.data = pixels;
        params.data_size = data_size;
        params.current_image_access = THSVS_ACCESS_NONE;
        params.height = height;
        params.width = width;
        params.next_image_access = THSVS_ACCESS_ANY_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER;
        vulkan.copy_data_to_image(params);


        texture_desc_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    }

    void GUI::create_descriptors()
    {
        std::vector<vk::DescriptorPoolSize> pool_sizes = {
            vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 1),
        };

        vk::DescriptorPoolCreateInfo dpci{};
        dpci.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
        dpci.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        dpci.pPoolSizes = pool_sizes.data();
        dpci.maxSets = NUM_VIRTUAL_FRAME;
        desc_pool = vulkan.device->createDescriptorPoolUnique(dpci);

        std::vector<vk::DescriptorSetLayoutBinding> bindings = {
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment)
        };

        desc_layout = vulkan.create_descriptor_layout(bindings);

        vk::DescriptorSetAllocateInfo dsai{};
        dsai.descriptorPool = desc_pool.get();
        dsai.pSetLayouts = &desc_layout.get();
        dsai.descriptorSetCount = 1;
        desc_set = std::move(vulkan.device->allocateDescriptorSetsUnique(dsai)[0]);

        vk::WriteDescriptorSet write;
        write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        write.descriptorCount = 1;
        write.dstSet = desc_set.get();
        write.dstBinding = 0;
        write.pImageInfo = &texture_desc_info;

        vulkan.device->updateDescriptorSets({ write }, nullptr);
    }

    void GUI::create_pipeline_layout()
    {
        vk::PipelineLayoutCreateInfo ci{};
        ci.pSetLayouts = &desc_layout.get();
        ci.setLayoutCount = 1;

        vk::PushConstantRange pcr{ vk::ShaderStageFlagBits::eVertex, 0, 4 * sizeof(float) };
        ci.pushConstantRangeCount = 1;
        ci.pPushConstantRanges = &pcr;

        pipeline_layout = vulkan.device->createPipelineLayoutUnique(ci);
    }


    void GUI::create_graphics_pipeline()
    {
        auto vert_code = tools::readFile("build/shaders/gui.vert.spv");
        auto frag_code = tools::readFile("build/shaders/gui.frag.spv");

        auto vert_module = vulkan.create_shader_module(vert_code);
        auto frag_module = vulkan.create_shader_module(frag_code);

        std::vector<vk::DynamicState> dynamic_states = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };

        vk::PipelineDynamicStateCreateInfo dyn_i{ {}, static_cast<uint32_t>(dynamic_states.size()), dynamic_states.data() };

        std::array<vk::VertexInputBindingDescription, 1> bindings;
        bindings[0].binding = 0;
        bindings[0].stride = sizeof(ImDrawVert);
        bindings[0].inputRate = vk::VertexInputRate::eVertex;


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#pragma clang diagnostic ignored "-Wold-style-cast"

        std::array<vk::VertexInputAttributeDescription, 3> attributes;
        attributes[0].binding = bindings[0].binding;
        attributes[0].location = 0;
        attributes[0].format = vk::Format::eR32G32Sfloat;
        attributes[0].offset = reinterpret_cast<size_t>(&((ImDrawVert*)0)->pos);

        attributes[1].binding = bindings[0].binding;
        attributes[1].location = 1;
        attributes[1].format = vk::Format::eR32G32Sfloat;
        attributes[1].offset = reinterpret_cast<size_t>(&((ImDrawVert*)0)->uv);

        attributes[2].binding = bindings[0].binding;
        attributes[2].location = 2;
        attributes[2].format = vk::Format::eR8G8B8A8Unorm;
        attributes[2].offset = reinterpret_cast<size_t>(&((ImDrawVert*)0)->col);

#pragma clang diagnostic pop

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
        colorblend_i.attachmentCount = att_states.size();
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
        ds_i.depthTestEnable = VK_FALSE;
        ds_i.depthWriteEnable = VK_FALSE;
        ds_i.depthCompareOp = vk::CompareOp::eAlways;

        vk::PipelineMultisampleStateCreateInfo ms_i{};
        ms_i.flags = vk::PipelineMultisampleStateCreateFlags();
        ms_i.pSampleMask = nullptr;
        ms_i.rasterizationSamples = vk::SampleCountFlagBits::e1;
        ms_i.sampleShadingEnable = VK_FALSE;
        ms_i.alphaToCoverageEnable = VK_FALSE;
        ms_i.alphaToOneEnable = VK_FALSE;
        ms_i.minSampleShading = .2f;

        std::vector<vk::PipelineShaderStageCreateInfo> shader_stages = {
            vk::PipelineShaderStageCreateInfo(
                vk::PipelineShaderStageCreateFlags(0),
                vk::ShaderStageFlagBits::eVertex,
                *vert_module,
                "main"),

            vk::PipelineShaderStageCreateInfo(
                vk::PipelineShaderStageCreateFlags(0),
                vk::ShaderStageFlagBits::eFragment,
                *frag_module,
                "main")
        };

        vk::GraphicsPipelineCreateInfo pipe_i{};
        pipe_i.layout = pipeline_layout.get();
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

        pipeline_cache = vulkan.device->createPipelineCacheUnique({});
        pipeline = vulkan.device->createGraphicsPipelineUnique(pipeline_cache.get(), pipe_i);
    }

}    // namespace my_app
