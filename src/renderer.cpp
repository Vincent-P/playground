#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vk_mem_alloc.h>

#include "renderer.hpp"

namespace my_app
{
    Renderer::Renderer(GLFWwindow* window)
        : ctx_(window)
          , model_("models/Sponza/glTF/Sponza.gltf", ctx_)
          // , model_("models/OrientationTest/glTF/OrientationTest.gltf", ctx_)
          // , model_("models/Box/glTF/Box.gltf", ctx_)
    {
        CreateSwapchain();
        CreateCommandPoolAndBuffers();
        CreateSemaphores();
        CreateDepthBuffer();
        CreateUniformBuffer();
        CreateDescriptors();
        CreateRenderPass();
        CreateFrameBuffers();
        CreateVertexBuffer();
        CreateIndexBuffer();
        LoadShaders();
        CreateGraphicsPipeline();
        FillCommandBuffers();
    }

    Renderer::~Renderer()
    {
        for (auto& o : swapchain_image_views)
            ctx_.device->destroy(o);
        ctx_.device->destroy(depth_image_view);

        for (auto& o : frame_buffers)
            ctx_.device->destroy(o);

        for (auto& o : command_buffers_fences)
            ctx_.device->destroy(o);

        for (auto& o : acquire_semaphores)
            ctx_.device->destroy(o);

        for (auto& o : render_complete_semaphores)
            ctx_.device->destroy(o);

        ctx_.device->destroy(scene_desc_layout);
        ctx_.device->destroy(node_desc_layout);

        ctx_.device->destroy(desc_pool);
        ctx_.device->destroy(command_pool);

        ctx_.device->destroy(vert_module);
        ctx_.device->destroy(frag_module);

        ctx_.device->destroy(pipeline_cache);
        ctx_.device->destroy(pipeline_layout);
        ctx_.device->destroy(pipeline);

        ctx_.device->destroy(render_pass);

        vertex_buffer.Free();
        index_buffer.Free();
        for (auto& ub : uniform_buffers)
            ub.Free();
        depth_image.Free();
        model_.Free();

        vmaDestroyAllocator(ctx_.allocator);
    }

    void Renderer::CreateSwapchain()
    {
        auto capabilities = ctx_.physical_device.getSurfaceCapabilitiesKHR(*ctx_.surface);
        auto extent = capabilities.currentExtent;

        auto present_modes = ctx_.physical_device.getSurfacePresentModesKHR(*ctx_.surface);
        auto present_mode = vk::PresentModeKHR::eFifo;

        for (auto& pm : present_modes)
        {
            if (pm == vk::PresentModeKHR::eMailbox)
            {
                present_mode = vk::PresentModeKHR::eMailbox;
                break;
            }
        }

        if (present_mode == vk::PresentModeKHR::eFifo)
        {
            for (auto& pm : present_modes)
            {
                if (pm == vk::PresentModeKHR::eImmediate)
                {
                    present_mode = vk::PresentModeKHR::eImmediate;
                    break;
                }
            }
        }

        auto formats = ctx_.physical_device.getSurfaceFormatsKHR(*ctx_.surface);
        auto format = formats[0];
        if (format.format == vk::Format::eUndefined)
        {
            format.format = vk::Format::eB8G8R8A8Unorm;
            format.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
        }
        else
        {
            for (const auto& f : formats)
            {
                if (f.format == vk::Format::eB8G8R8A8Unorm &&
                    f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
                {
                    format = f;
                    break;
                }
            }
        }

        assert(capabilities.maxImageCount >= NUM_FRAME_DATA);
        auto ci = vk::SwapchainCreateInfoKHR();
        ci.surface = *ctx_.surface;
        ci.minImageCount = NUM_FRAME_DATA;
        ci.imageFormat = format.format;
        ci.imageColorSpace = format.colorSpace;
        ci.imageExtent = extent;
        ci.imageArrayLayers = 1;
        ci.imageUsage =
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;

        if (ctx_.graphics_family_idx != ctx_.present_family_idx)
        {
            uint32_t indices[] = {(uint32_t)ctx_.graphics_family_idx,
                                  (uint32_t)ctx_.present_family_idx};
            ci.imageSharingMode = vk::SharingMode::eConcurrent;
            ci.queueFamilyIndexCount = 2;
            ci.pQueueFamilyIndices = indices;
        }
        else
        {
            ci.imageSharingMode = vk::SharingMode::eExclusive;
        }

        ci.preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
        ci.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        ci.presentMode = present_mode;
        ci.clipped = VK_TRUE;

        swapchain = ctx_.device->createSwapchainKHRUnique(ci);
        swapchain_images = ctx_.device->getSwapchainImagesKHR(*swapchain);
        swapchain_format = format.format;
        swapchain_present_mode = present_mode;
        swapchain_extent = extent;

        swapchain_image_views.clear();
        for (const auto& image : swapchain_images)
        {
            auto ci = vk::ImageViewCreateInfo();
            ci.image = image;
            ci.viewType = vk::ImageViewType::e2D;
            ci.format = swapchain_format;
            ci.components.r = vk::ComponentSwizzle::eR;
            ci.components.g = vk::ComponentSwizzle::eG;
            ci.components.b = vk::ComponentSwizzle::eB;
            ci.components.a = vk::ComponentSwizzle::eA;
            ci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            ci.subresourceRange.baseMipLevel = 0;
            ci.subresourceRange.levelCount = 1;
            ci.subresourceRange.baseArrayLayer = 0;
            ci.subresourceRange.layerCount = 1;
            ci.flags = {};
            auto view = ctx_.device->createImageView(ci);
            swapchain_image_views.push_back(view);
        }
    }

    void Renderer::CreateCommandPoolAndBuffers()
    {
        vk::CommandPoolCreateInfo ci;
        ci.flags = vk::CommandPoolCreateFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
        ci.queueFamilyIndex = ctx_.graphics_family_idx;

        command_pool = ctx_.device->createCommandPool(ci);

        vk::CommandBufferAllocateInfo cbai;
        cbai.commandPool = command_pool;
        cbai.level = vk::CommandBufferLevel::ePrimary;
        cbai.commandBufferCount = NUM_FRAME_DATA;

        command_buffers = ctx_.device->allocateCommandBuffers(cbai);

        command_buffers_fences.resize(command_buffers.size());
        for (int i = 0; i < command_buffers.size(); i++)
        {
            command_buffers_fences[i] = ctx_.device->createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
        }
    }

    void Renderer::CreateSemaphores()
    {
        acquire_semaphores.resize(NUM_FRAME_DATA);
        render_complete_semaphores.resize(NUM_FRAME_DATA);

        for (int i = 0; i < NUM_FRAME_DATA; ++i)
        {
            acquire_semaphores[i] =
                ctx_.device->createSemaphore(vk::SemaphoreCreateInfo());
            render_complete_semaphores[i] =
                ctx_.device->createSemaphore(vk::SemaphoreCreateInfo());
        }
    }

    void Renderer::CreateDepthBuffer()
    {
        std::vector<vk::Format> depthFormats = {
            vk::Format::eD32SfloatS8Uint, vk::Format::eD32Sfloat, vk::Format::eD24UnormS8Uint,
            vk::Format::eD16UnormS8Uint, vk::Format::eD16Unorm};

        for (auto& format : depthFormats)
        {
            auto depthFormatProperties = ctx_.physical_device.getFormatProperties(format);
            // Format must support depth stencil attachment for optimal tiling
            if (depthFormatProperties.optimalTilingFeatures &
                vk::FormatFeatureFlagBits::eDepthStencilAttachment)
            {
                depth_format = format;
                break;
            }
        }

        auto ci = vk::ImageCreateInfo();
        ci.imageType = vk::ImageType::e2D;
        ci.format = depth_format;
        ci.extent.width = swapchain_extent.width;
        ci.extent.height = swapchain_extent.height;
        ci.extent.depth = 1;
        ci.mipLevels = 1;
        ci.arrayLayers = 1;
        ci.samples = vk::SampleCountFlagBits::e1;
        ci.initialLayout = vk::ImageLayout::eUndefined;
        ci.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
        ci.queueFamilyIndexCount = 0;
        ci.pQueueFamilyIndices = NULL;
        ci.sharingMode = vk::SharingMode::eExclusive;
        ci.flags = {};

        depth_image = Image(ci, VMA_MEMORY_USAGE_GPU_ONLY, ctx_.allocator);

        auto vci = vk::ImageViewCreateInfo();
        vci.flags = {};
        vci.image = depth_image.GetImage();
        vci.format = depth_format;
        vci.components.r = vk::ComponentSwizzle::eR;
        vci.components.g = vk::ComponentSwizzle::eG;
        vci.components.b = vk::ComponentSwizzle::eB;
        vci.components.a = vk::ComponentSwizzle::eA;
        vci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        vci.subresourceRange.baseMipLevel = 0;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.baseArrayLayer = 0;
        vci.subresourceRange.layerCount = 1;
        vci.viewType = vk::ImageViewType::e2D;

        depth_image_view = ctx_.device->createImageView(vci);
    }

    void Renderer::CreateUniformBuffer()
    {
        uniform_buffers.resize(swapchain_images.size());

        for (auto& uniform_buffer : uniform_buffers)
        {
            auto buf_usage = vk::BufferUsageFlagBits::eUniformBuffer;
            auto mem_usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            uniform_buffer = Buffer(sizeof(MVP), buf_usage, mem_usage, ctx_.allocator);
        }
    }

    void Renderer::UpdateUniformBuffer(Buffer& uniform_buffer, float time, Camera& camera)
    {
        // transformation, angle, rotations axis
        MVP ubo;
        ubo.model = glm::mat4(1.0f);

        ubo.view = glm::lookAt(
            camera.position,                   // origin of camera
            camera.position + camera.front,    // where to look
            camera.up);

        ubo.proj = glm::infinitePerspective(
            glm::radians(45.0f),
            swapchain_extent.width / (float)swapchain_extent.height,
            0.1f);

        // Vulkan clip space has inverted Y and half Z.
        ubo.clip = glm::mat4(1.0f,  0.0f, 0.0f, 0.0f,
                             0.0f, -1.0f, 0.0f, 0.0f,
                             0.0f,  0.0f, 0.5f, 0.0f,
                             0.0f,  0.0f, 0.5f, 1.0f);

        void* mappedData = uniform_buffer.Map();
        memcpy(mappedData, &ubo, sizeof(ubo));
        uniform_buffer.Unmap();
    }

    void Renderer::CreateDescriptors()
    {
        std::vector<vk::DescriptorPoolSize> pool_sizes =
            {
                // MVP uniform + each mesh transforms
                vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, (1 + model_.meshes.size()) * swapchain_images.size())

            };

        vk::DescriptorPoolCreateInfo dpci{};
        dpci.poolSizeCount = pool_sizes.size();
        dpci.pPoolSizes = pool_sizes.data();
        dpci.maxSets = (1 + model_.meshes.size()) * swapchain_images.size();
        desc_pool = ctx_.device->createDescriptorPool(dpci);

        // Binding 0: Uniform buffer with scene informations (MVP)
        {
            std::vector<vk::DescriptorSetLayoutBinding> bindings =
                {
                    {0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr}

                };

            vk::DescriptorSetLayoutCreateInfo dslci{};
            dslci.bindingCount = bindings.size();
            dslci.pBindings = bindings.data();
            scene_desc_layout = ctx_.device->createDescriptorSetLayout(dslci);

            auto layouts = std::vector<vk::DescriptorSetLayout>(swapchain_images.size(), scene_desc_layout);
            vk::DescriptorSetAllocateInfo dsai{};
            dsai.descriptorPool = desc_pool;
            dsai.pSetLayouts = layouts.data();
            dsai.descriptorSetCount = layouts.size();
            desc_sets = ctx_.device->allocateDescriptorSets(dsai);

            for (size_t i = 0; i < swapchain_images.size(); i++)
            {
                auto dbi = uniform_buffers[i].GetDescInfo();

                std::array<vk::WriteDescriptorSet, 1> writes;
                writes[0].dstSet = desc_sets[i];
                writes[0].descriptorCount = 1;
                writes[0].descriptorType = vk::DescriptorType::eUniformBuffer;
                writes[0].pBufferInfo = &dbi;
                writes[0].dstArrayElement = 0;
                writes[0].dstBinding = 0;

                ctx_.device->updateDescriptorSets(writes, nullptr);
            }
        }

        // Binding 1: Nodes uniform (local transforms of each mesh)
        {
            std::vector<vk::DescriptorSetLayoutBinding> bindings =
                {
                    {0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr}

                };

            vk::DescriptorSetLayoutCreateInfo dslci{};
            dslci.bindingCount = bindings.size();
            dslci.pBindings = bindings.data();
            node_desc_layout = ctx_.device->createDescriptorSetLayout(dslci);

            for (auto& node : model_.scene_nodes)
                node.SetupNodeDescriptorSet(desc_pool, node_desc_layout, *ctx_.device);
        }
    }


    void Renderer::CreateRenderPass()
    {
        std::array<vk::AttachmentDescription, 2> attachments;

        attachments[0].format = swapchain_format;
        attachments[0].samples = vk::SampleCountFlagBits::e1;
        attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
        attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
        attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        attachments[0].initialLayout = vk::ImageLayout::eUndefined;
        attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;
        attachments[0].flags = vk::AttachmentDescriptionFlags();

        attachments[1].format = depth_format;
        attachments[1].samples = vk::SampleCountFlagBits::e1;
        attachments[1].loadOp = vk::AttachmentLoadOp::eClear;
        attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
        attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        attachments[1].initialLayout = vk::ImageLayout::eUndefined;
        attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        attachments[1].flags = vk::AttachmentDescriptionFlags();

        auto color_ref = vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal);
        auto depth_ref = vk::AttachmentReference(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);

        vk::SubpassDescription subpass = {};
        subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpass.flags = vk::SubpassDescriptionFlags();
        subpass.inputAttachmentCount = 0;
        subpass.pInputAttachments = nullptr;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_ref;
        subpass.pResolveAttachments = nullptr;
        subpass.pDepthStencilAttachment = &depth_ref;
        subpass.preserveAttachmentCount = 0;
        subpass.pPreserveAttachments = nullptr;

        vk::RenderPassCreateInfo rp_info = {};
        rp_info.attachmentCount = attachments.size();
        rp_info.pAttachments = attachments.data();
        rp_info.subpassCount = 1;
        rp_info.pSubpasses = &subpass;
        rp_info.dependencyCount = 0;
        rp_info.pDependencies = nullptr;
        render_pass = ctx_.device->createRenderPass(rp_info);
    }

    void Renderer::CreateFrameBuffers()
    {
        std::array<vk::ImageView, 2> attachments;
        attachments[1] = depth_image_view;

        frame_buffers.resize(swapchain_images.size());

        vk::FramebufferCreateInfo ci;
        ci.renderPass = render_pass;
        ci.attachmentCount = attachments.size();
        ci.pAttachments = attachments.data();
        ci.width = swapchain_extent.width;
        ci.height = swapchain_extent.height;
        ci.layers = 1;

        for (size_t i = 0; i < swapchain_image_views.size(); i++)
        {
            attachments[0] = swapchain_image_views[i];
            frame_buffers[i] = ctx_.device->createFramebuffer(ci);
        }
    }

    void Renderer::CreateIndexBuffer()
    {
        auto size = model_.indices.size() * sizeof(std::uint32_t);
        auto buf_usage = vk::BufferUsageFlagBits::eIndexBuffer;
        auto mem_usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        index_buffer = Buffer(size, buf_usage, mem_usage, ctx_.allocator);

        void* mappedData = index_buffer.Map();
        memcpy(mappedData, model_.indices.data(), size);
        index_buffer.Unmap();
    }

    void Renderer::CreateVertexBuffer()
    {
        auto size = model_.vertices.size() * sizeof(Vertex);
        auto buf_usage = vk::BufferUsageFlagBits::eVertexBuffer;
        auto mem_usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        vertex_buffer = Buffer(size, buf_usage, mem_usage, ctx_.allocator);

        void* mappedData = vertex_buffer.Map();
        memcpy(mappedData, model_.vertices.data(), size);
        vertex_buffer.Unmap();
    }

    std::vector<char> readFile(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open())
            throw std::runtime_error("failed to open file!");

        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }

    void Renderer::LoadShaders()
    {
        std::vector<char> vert_code = readFile("shaders/vert.spv");
        std::vector<char> frag_code = readFile("shaders/frag.spv");

        auto vert_i = vk::ShaderModuleCreateInfo();
        vert_i.codeSize = vert_code.size();
        vert_i.pCode = reinterpret_cast<const uint32_t*>(vert_code.data());
        vert_module = ctx_.device->createShaderModule(vert_i);

        auto frag_i = vk::ShaderModuleCreateInfo();
        frag_i.codeSize = frag_code.size();
        frag_i.pCode = reinterpret_cast<const uint32_t*>(frag_code.data());
        frag_module = ctx_.device->createShaderModule(frag_i);

        std::vector<vk::PipelineShaderStageCreateInfo> pipelineShaderStages = {
            vk::PipelineShaderStageCreateInfo(
                vk::PipelineShaderStageCreateFlags(),
                vk::ShaderStageFlagBits::eVertex,
                vert_module,
                "main",
                nullptr),

            vk::PipelineShaderStageCreateInfo(
                vk::PipelineShaderStageCreateFlags(),
                vk::ShaderStageFlagBits::eFragment,
                frag_module,
                "main",
                nullptr)};

        shader_stages = pipelineShaderStages;
    }

    void Renderer::CreateGraphicsPipeline()
    {
        std::vector<vk::DynamicState> dynamic_states =
            {
                vk::DynamicState::eViewport,
                vk::DynamicState::eScissor};

        vk::PipelineDynamicStateCreateInfo dyn_i(
            vk::PipelineDynamicStateCreateFlags(),
            dynamic_states.size(),
            dynamic_states.data());

        auto bindings = Vertex::getBindingDescriptions();
        auto attributes = Vertex::getAttributeDescriptions();
        vk::PipelineVertexInputStateCreateInfo vert_i;
        vert_i.flags = vk::PipelineVertexInputStateCreateFlags();
        vert_i.vertexBindingDescriptionCount = bindings.size();
        vert_i.pVertexBindingDescriptions = bindings.data();
        vert_i.vertexAttributeDescriptionCount = attributes.size();
        vert_i.pVertexAttributeDescriptions = attributes.data();

        vk::PipelineInputAssemblyStateCreateInfo asm_i;
        asm_i.flags = vk::PipelineInputAssemblyStateCreateFlags();
        asm_i.primitiveRestartEnable = VK_FALSE;
        asm_i.topology = vk::PrimitiveTopology::eTriangleList;

        vk::PipelineRasterizationStateCreateInfo rast_i;
        rast_i.flags = vk::PipelineRasterizationStateCreateFlags();
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
        att_states[0].colorWriteMask = vk::ColorComponentFlags(
            vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA);
        att_states[0].blendEnable = VK_TRUE;
        att_states[0].srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
        att_states[0].dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
        att_states[0].colorBlendOp = vk::BlendOp::eAdd;
        att_states[0].srcAlphaBlendFactor = vk::BlendFactor::eOne;
        att_states[0].dstAlphaBlendFactor = vk::BlendFactor::eZero;
        att_states[0].alphaBlendOp = vk::BlendOp::eAdd;

        vk::PipelineColorBlendStateCreateInfo colorblend_i;
        colorblend_i.flags = vk::PipelineColorBlendStateCreateFlags();
        colorblend_i.attachmentCount = att_states.size();
        colorblend_i.pAttachments = att_states.data();
        colorblend_i.logicOpEnable = VK_FALSE;
        colorblend_i.logicOp = vk::LogicOp::eCopy;
        colorblend_i.blendConstants[0] = 0.0f;
        colorblend_i.blendConstants[1] = 0.0f;
        colorblend_i.blendConstants[2] = 0.0f;
        colorblend_i.blendConstants[3] = 0.0f;

        vk::PipelineViewportStateCreateInfo vp_i;
        vp_i.flags = vk::PipelineViewportStateCreateFlags();
        vp_i.viewportCount = 1;
        vp_i.scissorCount = 1;
        vp_i.pScissors = nullptr;
        vp_i.pViewports = nullptr;

        vk::PipelineDepthStencilStateCreateInfo ds_i;
        ds_i.flags = vk::PipelineDepthStencilStateCreateFlags();
        ds_i.depthTestEnable = VK_TRUE;
        ds_i.depthWriteEnable = VK_TRUE;
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

        vk::PipelineMultisampleStateCreateInfo ms_i;
        ms_i.flags = vk::PipelineMultisampleStateCreateFlags();
        ms_i.pSampleMask = nullptr;
        ms_i.rasterizationSamples = vk::SampleCountFlagBits::e1;
        ms_i.sampleShadingEnable = VK_FALSE;
        ms_i.alphaToCoverageEnable = VK_FALSE;
        ms_i.alphaToOneEnable = VK_FALSE;
        ms_i.minSampleShading = 0.0;

        std::array<vk::DescriptorSetLayout, 2> layouts =
            {
                scene_desc_layout, node_desc_layout

            };
        auto ci = vk::PipelineLayoutCreateInfo();
        ci.pSetLayouts = layouts.data();
        ci.setLayoutCount = layouts.size();

        auto pcr = vk::PushConstantRange(vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstBlockMaterial));
        ci.pushConstantRangeCount = 1;
        ci.pPushConstantRanges = &pcr;

        pipeline_layout = ctx_.device->createPipelineLayout(ci);

        vk::GraphicsPipelineCreateInfo pipe_i;
        pipe_i.layout = pipeline_layout;
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
        pipe_i.stageCount = shader_stages.size();
        pipe_i.renderPass = render_pass;
        pipe_i.subpass = 0;

        pipeline_cache = ctx_.device->createPipelineCache(vk::PipelineCacheCreateInfo());
        pipeline = ctx_.device->createGraphicsPipeline(pipeline_cache, pipe_i);
    }

    void Renderer::FillCommandBuffers()
    {
        auto renderArea = vk::Rect2D(vk::Offset2D(), swapchain_extent);

        std::array<vk::ClearValue, 2> clearValues = {};
        clearValues[0].color = vk::ClearColorValue(std::array<float, 4>{0.5f, 0.5f, 0.5f, 1.0f});
        clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

        // From here we can do common GL commands
        // Lets add commands to each command buffer.
        for (int32_t i = 0; i < command_buffers.size(); ++i)
        {
            command_buffers[i].begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eSimultaneousUse));

            command_buffers[i].beginRenderPass(
                vk::RenderPassBeginInfo(
                    render_pass,
                    frame_buffers[i],
                    renderArea,
                    clearValues.size(),
                    clearValues.data()),
                vk::SubpassContents::eInline);

            std::vector<vk::Viewport> viewports;
            viewports.emplace_back(
                0,
                0,
                swapchain_extent.width,
                swapchain_extent.height,
                0,
                1.0f);

            if (current_resize_)
            {
                viewports[0].setWidth(current_resize_->first);
                viewports[0].setHeight(current_resize_->second);
                current_resize_ = std::nullopt;
            }

            command_buffers[i].setViewport(0, viewports);

            std::vector<vk::Rect2D> scissors = {renderArea};

            command_buffers[i].setScissor(0, scissors);

            // Bind Descriptor Sets, these are attribute/uniform "descriptions"
            command_buffers[i].bindPipeline(
                vk::PipelineBindPoint::eGraphics,
                pipeline);

            vk::Buffer vertexBuffers[] = {vertex_buffer.GetBuffer()};
            vk::DeviceSize offsets[] = {0};
            command_buffers[i].bindVertexBuffers(0, 1, vertexBuffers, offsets);
            command_buffers[i].bindIndexBuffer(index_buffer.GetBuffer(), 0, vk::IndexType::eUint32);

            model_.draw(command_buffers[i], pipeline_layout, desc_sets[i]);

            command_buffers[i].endRenderPass();
            command_buffers[i].end();
        }
    }

    void Renderer::Resize(int width, int height)
    {
        current_resize_ = std::make_optional<std::pair<int, int>>(width, height);
    }

    void Renderer::DrawFrame(double time, Camera& camera)
    {
        static uint32_t currentBuffer = 0;
        static uint32_t imageIndex = 0;

        auto& device = ctx_.device;
        auto graphicsQueue = device->getQueue(ctx_.graphics_family_idx, 0);


        device->waitForFences(1,
                              &command_buffers_fences[currentBuffer],
                              VK_TRUE,
                              UINT64_MAX);

        device->resetFences(1, &command_buffers_fences[currentBuffer]);

        // will signal acquire_semaphore when the next image is acquired
        // and put its index in imageIndex
        device->acquireNextImageKHR(*swapchain,
                                    std::numeric_limits<uint64_t>::max(),
                                    acquire_semaphores[currentBuffer],
                                    nullptr,
                                    &imageIndex);

        UpdateUniformBuffer(uniform_buffers[imageIndex], time, camera);

        if (current_resize_)
        {

            std::vector<vk::Viewport> viewports;
            viewports.emplace_back(
                0,
                0,
                current_resize_->first,
                current_resize_->second,
                0,
                1.0f);

            current_resize_ = std::nullopt;

            command_buffers[imageIndex].begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eSimultaneousUse));
            command_buffers[imageIndex].setViewport(0, viewports);
            command_buffers[imageIndex].end();
        }


        // Create kernels to submit to the queue on a given render pass.
        vk::PipelineStageFlags stages[] = {
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
        };

        auto kernel = vk::SubmitInfo();
        kernel.waitSemaphoreCount = 1;
        kernel.pWaitSemaphores = &acquire_semaphores[currentBuffer];
        kernel.pWaitDstStageMask = stages;
        kernel.commandBufferCount = 1;
        kernel.pCommandBuffers = &command_buffers[imageIndex];
        kernel.signalSemaphoreCount = 1;
        kernel.pSignalSemaphores = &render_complete_semaphores[currentBuffer];

        // the fences will be signaled
        graphicsQueue.submit(1, &kernel, command_buffers_fences[currentBuffer]);

        auto present_i = vk::PresentInfoKHR();
        present_i.waitSemaphoreCount = 1;
        present_i.pWaitSemaphores = &render_complete_semaphores[currentBuffer];
        present_i.swapchainCount = 1;
        present_i.pSwapchains = &swapchain.get();
        present_i.pImageIndices = &imageIndex;
        graphicsQueue.presentKHR(present_i);

        currentBuffer = (currentBuffer + 1) % NUM_FRAME_DATA;
    }

    void Renderer::WaitIdle()
    {
        ctx_.device->waitIdle();
    }
}    // namespace my_app
