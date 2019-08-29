#pragma clang diagnostic ignored "-Weverything"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vk_mem_alloc.h>
#include <imgui.h>
#pragma clang diagnostic pop

#include "renderer.hpp"
#include "timer.hpp"
#include "tools.hpp"

namespace my_app
{
    Renderer::Renderer(GLFWwindow* window, std::string model_path)
        : vulkan(window)
        , model(model_path, vulkan)
        , gui(*this)
    {
        tools::start_log("[RENDERER] Creating a dummy texture");
        auto start = clock_t::now();

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
        ci.pQueueFamilyIndices = NULL;
        ci.sharingMode = vk::SharingMode::eExclusive;
        ci.flags = {};
        empty_image = Image{ "Empty image", vulkan.allocator, ci };

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
        vk::ImageViewCreateInfo vci{};
        vci.flags = {};
        vci.image = empty_image.get_image();
        vci.format = format;
        vci.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
        vci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        vci.subresourceRange.baseMipLevel = 0;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.baseArrayLayer = 0;
        vci.subresourceRange.layerCount = 1;
        vci.viewType = vk::ImageViewType::e2D;
        empty_info.imageView = vulkan.device->createImageView(vci);

        vk::ImageSubresourceRange subresource_range{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

        vk::ImageMemoryBarrier b{};
        b.oldLayout = vk::ImageLayout::eUndefined;
        b.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        b.srcAccessMask = {};
        b.dstAccessMask = vk::AccessFlagBits::eTransferRead;
        b.image = empty_image.get_image();
        b.subresourceRange = subresource_range;

        vulkan.transition_layout(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, b);
        empty_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        // Create the swapchain
        tools::log(start, "[RENDERER] Creating a swapchain");
        create_swapchain();

        tools::log(start, "[RENDERER] Creating the descriptor sets");
        create_descriptors();

        // Create ressources
        tools::log(start, "[RENDERER] Creating the frame resources");
        create_frame_ressources();
        tools::log(start, "[RENDERER] Creating the color buffer");
        create_color_buffer();
        tools::log(start, "[RENDERER] Creating the depth buffer");
        create_depth_buffer();

        tools::log(start, "[RENDERER] Creating the vertex and index buffer");
        create_vertex_buffer();
        create_index_buffer();


        // Create the pipeline
        tools::log(start, "[RENDERER] Creating the render pass");
        create_render_pass();
        tools::log(start, "[RENDERER] Creating the graphics pipeline");
        create_graphics_pipeline();
        tools::end_log(start, "[RENDERER] Done, now initializing the GUI!");
        gui.init();
    }

    Renderer::~Renderer()
    {
        // EMPTY TEXTURE
        vulkan.device->destroy(empty_info.imageView);
        vulkan.device->destroy(empty_info.sampler);
        empty_image.free();

        // SWAPCHAIN OBJECTS
        destroy_swapchain();

        vertex_buffer.free();
        index_buffer.free();

        for (auto& frame_ressource : frame_resources)
            frame_ressource.uniform_buffer.free();

        model.free();
    }

    void Renderer::destroy_swapchain()
    {
        for (auto& o : swapchain.image_views)
            vulkan.device->destroy(o);

        vulkan.device->destroy(depth_image_view);
        vulkan.device->destroy(color_image_view);

        depth_image.free();
        color_image.free();
    }

    void Renderer::recreate_swapchain()
    {
        vulkan.device->waitIdle();
        destroy_swapchain();
        vulkan.device->waitIdle();

        create_swapchain();
        create_color_buffer();
        create_depth_buffer();
        create_render_pass();
        create_frame_ressources();
    }

    void Renderer::create_swapchain()
    {
        // Use default extent for the swapchain
        auto capabilities = vulkan.physical_device.getSurfaceCapabilitiesKHR(*vulkan.surface);

        swapchain.extent = capabilities.currentExtent;

        // Find a good present mode (by priority Mailbox then Immediate then FIFO)
        auto present_modes = vulkan.physical_device.getSurfacePresentModesKHR(*vulkan.surface);
        swapchain.present_mode = vk::PresentModeKHR::eFifo;

        for (auto& pm : present_modes)
            if (pm == vk::PresentModeKHR::eMailbox)
            {
                swapchain.present_mode = vk::PresentModeKHR::eMailbox;
                break;
            }

        if (swapchain.present_mode == vk::PresentModeKHR::eFifo)
            for (auto& pm : present_modes)
                if (pm == vk::PresentModeKHR::eImmediate)
                {
                    swapchain.present_mode = vk::PresentModeKHR::eImmediate;
                    break;
                }

        // Find the best format
        auto formats = vulkan.physical_device.getSurfaceFormatsKHR(*vulkan.surface);
        swapchain.format = formats[0];
        if (swapchain.format.format == vk::Format::eUndefined)
        {
            swapchain.format.format = vk::Format::eB8G8R8A8Unorm;
            swapchain.format.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
        }
        else
        {
            for (const auto& f : formats)
                if (f.format == vk::Format::eB8G8R8A8Unorm && f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
                {
                    swapchain.format = f;
                    break;
                }
        }

        assert(capabilities.maxImageCount == 0 or capabilities.maxImageCount >= NUM_VIRTUAL_FRAME);

        vk::SwapchainCreateInfoKHR ci{};
        ci.surface = *vulkan.surface;
        ci.minImageCount = capabilities.minImageCount + 1;
        ci.imageFormat = swapchain.format.format;
        ci.imageColorSpace = swapchain.format.colorSpace;
        ci.imageExtent = swapchain.extent;
        ci.imageArrayLayers = 1;
        ci.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;

        if (vulkan.graphics_family_idx != vulkan.present_family_idx)
        {
            uint32_t indices[] = {
                (uint32_t)vulkan.graphics_family_idx,
                (uint32_t)vulkan.present_family_idx
            };
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
        ci.presentMode = swapchain.present_mode;
        ci.clipped = VK_TRUE;

        swapchain.handle = vulkan.device->createSwapchainKHRUnique(ci);

        swapchain.images = vulkan.device->getSwapchainImagesKHR(*swapchain.handle);

        swapchain.image_views.resize(swapchain.images.size());

        for (size_t i = 0; i < swapchain.images.size(); i++)
        {
            vk::ImageViewCreateInfo ci{};
            ci.image = swapchain.images[i];
            ci.viewType = vk::ImageViewType::e2D;
            ci.format = swapchain.format.format;
            ci.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
            ci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            ci.subresourceRange.baseMipLevel = 0;
            ci.subresourceRange.levelCount = 1;
            ci.subresourceRange.baseArrayLayer = 0;
            ci.subresourceRange.layerCount = 1;
            swapchain.image_views[i] = vulkan.device->createImageView(ci);
        }
    }

    void Renderer::create_color_buffer()
    {
        vk::ImageCreateInfo ci{};
        ci.flags = {};
        ci.imageType = vk::ImageType::e2D;
        ci.format = swapchain.format.format;
        ci.extent.width = swapchain.extent.width;
        ci.extent.height = swapchain.extent.height;
        ci.extent.depth = 1;
        ci.mipLevels = 1;
        ci.arrayLayers = 1;
        ci.samples = MSAA_SAMPLES;
        ci.initialLayout = vk::ImageLayout::eUndefined;
        ci.usage = vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment;
        ci.queueFamilyIndexCount = 0;
        ci.pQueueFamilyIndices = NULL;
        ci.sharingMode = vk::SharingMode::eExclusive;

        color_image = Image{ "Color image", vulkan.allocator, ci };

        vk::ImageViewCreateInfo vci{};
        vci.flags = {};
        vci.image = color_image.get_image();
        vci.format = swapchain.format.format;
        vci.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
        vci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        vci.subresourceRange.baseMipLevel = 0;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.baseArrayLayer = 0;
        vci.subresourceRange.layerCount = 1;
        vci.viewType = vk::ImageViewType::e2D;

        color_image_view = vulkan.device->createImageView(vci);

        vk::ImageSubresourceRange subresource_range{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

        vk::ImageMemoryBarrier b{};
        b.oldLayout = vk::ImageLayout::eUndefined;
        b.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
        b.srcAccessMask = {};
        b.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
        b.image = color_image.get_image();
        b.subresourceRange = subresource_range;

        vulkan.transition_layout(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput, b);
    }


    void Renderer::create_depth_buffer()
    {
        std::vector<vk::Format> depth_formats = {
            vk::Format::eD32SfloatS8Uint, vk::Format::eD32Sfloat, vk::Format::eD24UnormS8Uint,
            vk::Format::eD16UnormS8Uint, vk::Format::eD16Unorm

        };

        for (auto& format : depth_formats)
        {
            auto depthFormatProperties = vulkan.physical_device.getFormatProperties(format);
            // Format must support depth stencil attachment for optimal tiling
            if (depthFormatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
            {
                depth_format = format;
                break;
            }
        }

        vk::ImageCreateInfo ci{};
        ci.flags = {};
        ci.imageType = vk::ImageType::e2D;
        ci.format = depth_format;
        ci.extent.width = swapchain.extent.width;
        ci.extent.height = swapchain.extent.height;
        ci.extent.depth = 1;
        ci.mipLevels = 1;
        ci.arrayLayers = 1;
        ci.samples = MSAA_SAMPLES;
        ci.initialLayout = vk::ImageLayout::eUndefined;
        ci.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
        ci.queueFamilyIndexCount = 0;
        ci.pQueueFamilyIndices = NULL;
        ci.sharingMode = vk::SharingMode::eExclusive;

        depth_image = Image{ "Depth image", vulkan.allocator, ci };

        vk::ImageViewCreateInfo vci{};
        vci.flags = {};
        vci.image = depth_image.get_image();
        vci.format = depth_format;
        vci.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
        vci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        vci.subresourceRange.baseMipLevel = 0;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.baseArrayLayer = 0;
        vci.subresourceRange.layerCount = 1;
        vci.viewType = vk::ImageViewType::e2D;

        depth_image_view = vulkan.device->createImageView(vci);
    }

    void Renderer::create_render_pass()
    {
        std::array<vk::AttachmentDescription, 3> attachments;

        // Color attachment
        attachments[0].format = swapchain.format.format;
        attachments[0].samples = MSAA_SAMPLES;
        attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
        attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
        attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        attachments[0].initialLayout = vk::ImageLayout::eUndefined;
        attachments[0].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
        attachments[0].flags = {};

        // Depth attachment
        attachments[1].format = depth_format;
        attachments[1].samples = MSAA_SAMPLES;
        attachments[1].loadOp = vk::AttachmentLoadOp::eClear;
        attachments[1].storeOp = vk::AttachmentStoreOp::eStore;
        attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        attachments[1].initialLayout = vk::ImageLayout::eUndefined;
        attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        attachments[1].flags = vk::AttachmentDescriptionFlags();

        // Color resolve attachment
        attachments[2].format = swapchain.format.format;
        attachments[2].samples = vk::SampleCountFlagBits::e1;
        attachments[2].loadOp = vk::AttachmentLoadOp::eDontCare;
        attachments[2].storeOp = vk::AttachmentStoreOp::eStore;
        attachments[2].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        attachments[2].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        attachments[2].initialLayout = vk::ImageLayout::eUndefined;
        attachments[2].finalLayout = vk::ImageLayout::ePresentSrcKHR;
        attachments[2].flags = {};

        vk::AttachmentReference color_ref(0, vk::ImageLayout::eColorAttachmentOptimal);
        vk::AttachmentReference depth_ref(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);
        vk::AttachmentReference color_resolve_ref(2, vk::ImageLayout::eColorAttachmentOptimal);

        std::array<vk::SubpassDescription, 2> subpasses{};
        subpasses[0].flags = vk::SubpassDescriptionFlags(0);
        subpasses[0].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpasses[0].inputAttachmentCount = 0;
        subpasses[0].pInputAttachments = nullptr;
        subpasses[0].colorAttachmentCount = 1;
        subpasses[0].pColorAttachments = &color_ref;
        subpasses[0].pResolveAttachments = &color_resolve_ref;
        subpasses[0].pDepthStencilAttachment = &depth_ref;
        subpasses[0].preserveAttachmentCount = 0;
        subpasses[0].pPreserveAttachments = nullptr;

        subpasses[1].flags = vk::SubpassDescriptionFlags(0);
        subpasses[1].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpasses[1].inputAttachmentCount = 0;
        subpasses[1].pInputAttachments = nullptr;
        subpasses[1].colorAttachmentCount = 1;
        subpasses[1].pColorAttachments = &color_resolve_ref;
        subpasses[1].pResolveAttachments = nullptr;
        subpasses[1].pDepthStencilAttachment = nullptr;
        subpasses[1].preserveAttachmentCount = 0;
        subpasses[1].pPreserveAttachments = nullptr;

        vk::RenderPassCreateInfo rp_info{};
        rp_info.attachmentCount = attachments.size();
        rp_info.pAttachments = attachments.data();
        rp_info.subpassCount = subpasses.size();
        rp_info.pSubpasses = subpasses.data();
        rp_info.dependencyCount = 0;
        rp_info.pDependencies = nullptr;
        render_pass = vulkan.device->createRenderPassUnique(rp_info);
    }

    void Renderer::update_uniform_buffer(FrameRessource* frame_ressource, Camera& camera)
    {
        // transformation, angle, rotations axis
        MVP ubo{};
        ubo.cam_pos = camera.position;

        float aspect_ratio = swapchain.extent.width / (float)swapchain.extent.height;

        ImGui::SetNextWindowPos(ImVec2(200.f, 20.0f));
        ImGui::Begin("Uniform buffer", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        static float fov = 45.0f;
        ImGui::DragFloat("FOV", &fov, 1.0f, 20.f, 90.f);

        ubo.proj = glm::perspective(glm::radians(fov), aspect_ratio, 0.1f, 500.0f);

        ubo.view = glm::lookAt(
            camera.position,                   // origin of camera
            camera.position + camera.front,    // where to look
            camera.up);

        // Vulkan clip space has inverted Y and half Z.
        ubo.clip = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f,
                             0.0f, -1.0f, 0.0f, 0.0f,
                             0.0f, 0.0f, 0.5f, 0.0f,
                             0.0f, 0.0f, 0.5f, 1.0f);

        ImGui::End();

        void* mappedData = frame_ressource->uniform_buffer.map();
        memcpy(mappedData, &ubo, sizeof(ubo));
    }

    void Renderer::create_descriptors()
    {
        std::vector<vk::DescriptorPoolSize> pool_sizes = {
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, (model.meshes.size()) + NUM_VIRTUAL_FRAME),
            vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 5 * model.materials.size())
        };

        vk::DescriptorPoolCreateInfo dpci{};
        dpci.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
        dpci.poolSizeCount = pool_sizes.size();
        dpci.pPoolSizes = pool_sizes.data();
        dpci.maxSets = NUM_VIRTUAL_FRAME + model.meshes.size() + model.materials.size();
        desc_pool = vulkan.device->createDescriptorPoolUnique(dpci);

        // Descriptor set 0: Scene/Camera informations (MVP)
        {
            std::vector<vk::DescriptorSetLayoutBinding> bindings = {
                vk::DescriptorSetLayoutBinding( 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex )
            };

            scene_desc_layout = vulkan.create_descriptor_layout(bindings);

            std::vector<vk::DescriptorSetLayout> layouts{ NUM_VIRTUAL_FRAME, scene_desc_layout.get() };

            vk::DescriptorSetAllocateInfo dsai{};
            dsai.descriptorPool = desc_pool.get();
            dsai.pSetLayouts = layouts.data();
            dsai.descriptorSetCount = layouts.size();
            desc_sets = vulkan.device->allocateDescriptorSetsUnique(dsai);

        }

        // Descriptor set 1: Materials
        {
            std::vector<vk::DescriptorSetLayoutBinding> bindings = {
                { 0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr },
                { 1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr },
                { 2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr },
                { 3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr },
                { 4, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr },
            };
            mat_desc_layout = vulkan.create_descriptor_layout(bindings);

            // Per-Material descriptor sets
            for (auto& material : model.materials)
            {
                // Allocate the descriptor set for the material
                vk::DescriptorSetAllocateInfo dsai{};
                dsai.descriptorPool = desc_pool.get();
                dsai.pSetLayouts = &mat_desc_layout.get();
                dsai.descriptorSetCount = 1;
                material.desc_set = vulkan.device->allocateDescriptorSets(dsai)[0];

                // Informations for each texture
                std::vector<vk::DescriptorImageInfo> image_descriptors = {
                    empty_info,
                    empty_info,
                    material.normal ? material.normal->desc_info : empty_info,
                    material.occlusion ? material.occlusion->desc_info : empty_info,
                    material.emissive ? material.emissive->desc_info : empty_info
                };

                // TODO: glTF specs states that metallic roughness should be preferred, even if specular glosiness is present
                if (material.workflow == Material::PbrWorkflow::MetallicRoughness)
                {
                    if (material.base_color)
                        image_descriptors[0] = material.base_color->desc_info;
                    if (material.metallic_roughness)
                        image_descriptors[1] = material.metallic_roughness->desc_info;
                }
                else
                {
                    if (material.extension.diffuse)
                        image_descriptors[0] = material.extension.diffuse->desc_info;
                    if (material.extension.specular_glosiness)
                        image_descriptors[1] = material.extension.specular_glosiness->desc_info;
                }

                // Fill then descriptor set with a binding for each texture
                std::array<vk::WriteDescriptorSet, 5> write_descriptor_sets{};
                for (uint32_t i = 0; i < image_descriptors.size(); i++)
                {
                    write_descriptor_sets[i].descriptorType = vk::DescriptorType::eCombinedImageSampler;
                    write_descriptor_sets[i].descriptorCount = 1;
                    write_descriptor_sets[i].dstSet = material.desc_set;
                    write_descriptor_sets[i].dstBinding = i;
                    write_descriptor_sets[i].pImageInfo = &image_descriptors[i];
                }

                vulkan.device->updateDescriptorSets(write_descriptor_sets, nullptr);
            }
        }

        // Descriptor set 2: Nodes uniform (local transforms of each mesh)
        {
            std::vector<vk::DescriptorSetLayoutBinding> bindings = {
                { 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr }
            };

            node_desc_layout = vulkan.create_descriptor_layout(bindings);

            for (auto& node : model.scene_nodes)
                node.setup_node_descriptor_set(desc_pool, node_desc_layout, vulkan.device);
        }
    }

    void Renderer::create_index_buffer()
    {
        auto size = model.indices.size() * sizeof(std::uint32_t);
        index_buffer = Buffer("Index buffer", vulkan.allocator, size, vk::BufferUsageFlagBits::eIndexBuffer);

        void* mappedData = index_buffer.map();
        memcpy(mappedData, model.indices.data(), size);
    }

    void Renderer::create_vertex_buffer()
    {
        auto size = model.vertices.size() * sizeof(Vertex);
        vertex_buffer = Buffer("Vertex buffer", vulkan.allocator, size, vk::BufferUsageFlagBits::eVertexBuffer);

        void* mappedData = vertex_buffer.map();
        memcpy(mappedData, model.vertices.data(), size);
    }

    void Renderer::create_frame_ressources()
    {
        for (auto& resource : frame_resources)
            resource.uniform_buffer.free();

        frame_resources.resize(NUM_VIRTUAL_FRAME);

        for (size_t i = 0; i < NUM_VIRTUAL_FRAME; i++)
        {
            auto frame_ressource = frame_resources.data() + i;

            vk::FenceCreateInfo fci{};
            frame_ressource->fence = vulkan.device->createFenceUnique({ vk::FenceCreateFlagBits::eSignaled });
            frame_ressource->image_available = vulkan.device->createSemaphoreUnique({});
            frame_ressource->rendering_finished = vulkan.device->createSemaphoreUnique({});

            frame_ressource->commandbuffer = std::move(vulkan.device->allocateCommandBuffersUnique({ vulkan.command_pool, vk::CommandBufferLevel::ePrimary, 1 })[0]);

            std::string name = "Uniform buffer ";
            name += std::to_string(i);
            frame_ressource->uniform_buffer = Buffer(name, vulkan.allocator, sizeof(MVP), vk::BufferUsageFlagBits::eUniformBuffer);

            auto dbi = frame_resources[i].uniform_buffer.get_desc_info();

            std::array<vk::WriteDescriptorSet, 1> writes;
            writes[0].dstSet = desc_sets[i].get();
            writes[0].descriptorCount = 1;
            writes[0].descriptorType = vk::DescriptorType::eUniformBuffer;
            writes[0].pBufferInfo = &dbi;
            writes[0].dstArrayElement = 0;
            writes[0].dstBinding = 0;

            vulkan.device->updateDescriptorSets(writes, nullptr);
        }
    }

    void Renderer::create_graphics_pipeline()
    {
        auto vert_code = tools::readFile("build/shaders/shader.vert.spv");
        auto frag_code = tools::readFile("build/shaders/shader.frag.spv");

        vert_module = vulkan.create_shader_module(vert_code);
        frag_module = vulkan.create_shader_module(frag_code);

        std::vector<vk::DynamicState> dynamic_states = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor

        };

        vk::PipelineDynamicStateCreateInfo dyn_i{ {}, static_cast<uint32_t>(dynamic_states.size()), dynamic_states.data() };

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

        vk::PipelineMultisampleStateCreateInfo ms_i{};
        ms_i.flags = vk::PipelineMultisampleStateCreateFlags();
        ms_i.pSampleMask = nullptr;
        ms_i.rasterizationSamples = MSAA_SAMPLES;
        ms_i.sampleShadingEnable = VK_TRUE;
        ms_i.alphaToCoverageEnable = VK_FALSE;
        ms_i.alphaToOneEnable = VK_FALSE;
        ms_i.minSampleShading = .2f;

        std::array<vk::DescriptorSetLayout, 3> layouts = {
            scene_desc_layout.get(), mat_desc_layout.get(), node_desc_layout.get()
        };
        vk::PipelineLayoutCreateInfo ci{};
        ci.pSetLayouts = layouts.data();
        ci.setLayoutCount = layouts.size();

        vk::PushConstantRange pcr{ vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstBlockMaterial) };
        ci.pushConstantRangeCount = 1;
        ci.pPushConstantRanges = &pcr;

        pipeline_layout = vulkan.device->createPipelineLayoutUnique(ci);

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
        pipe_i.stageCount = shader_stages.size();
        pipe_i.renderPass = render_pass.get();
        pipe_i.subpass = 0;

        pipeline_cache = vulkan.device->createPipelineCacheUnique({});
        pipeline = vulkan.device->createGraphicsPipelineUnique(pipeline_cache.get(), pipe_i);
    }

    void Renderer::resize(int, int)
    {
        recreate_swapchain();
    }

    void Renderer::draw_frame(Camera& camera, const TimerData& timer)
    {
        static uint32_t virtual_frame_idx = 0;

        gui.start_frame(timer);

        auto& device = vulkan.device;
        auto graphics_queue = vulkan.get_graphics_queue();
        auto frame_resource = frame_resources.data() + virtual_frame_idx;

        auto wait_result = device->waitForFences(frame_resource->fence.get(), VK_TRUE, 1000000000);
        if (wait_result == vk::Result::eTimeout)
        {
            throw std::runtime_error("Submitted the frame more than 1 second ago.");
        }

        device->resetFences(frame_resource->fence.get());

        uint32_t image_index = 0;
        auto result = device->acquireNextImageKHR(*swapchain.handle,
                                                  std::numeric_limits<uint64_t>::max(),
                                                  *frame_resource->image_available,
                                                  nullptr,
                                                  &image_index);

        if (result == vk::Result::eErrorOutOfDateKHR)
        {
            std::cerr << "The swapchain is out of date. Recreating it...\n";
            recreate_swapchain();
            return;
        }

        // Create the framebuffer for the frame
        {
            std::array<vk::ImageView, 3> attachments = {
                color_image_view,
                depth_image_view,
                swapchain.image_views[image_index]
            };

            vk::FramebufferCreateInfo ci{};
            ci.renderPass = render_pass.get();
            ci.attachmentCount = attachments.size();
            ci.pAttachments = attachments.data();
            ci.width = swapchain.extent.width;
            ci.height = swapchain.extent.height;
            ci.layers = 1;
            frame_resource->framebuffer = vulkan.device->createFramebufferUnique(ci);
        }

        // Update and Draw!!!
        {
            update_uniform_buffer(frame_resource, camera);

            vk::Rect2D render_area{ vk::Offset2D(), swapchain.extent };

            std::array<vk::ClearValue, 2> clear_values;
            clear_values[0].color = vk::ClearColorValue(std::array<float, 4>{ 0.6f, 0.7f, 0.94f, 1.0f });
            clear_values[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

            frame_resource->commandbuffer->begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

            vk::RenderPassBeginInfo rpbi{};
            rpbi.renderArea = render_area;
            rpbi.renderPass = render_pass.get();
            rpbi.framebuffer = *frame_resource->framebuffer;
            rpbi.clearValueCount = clear_values.size();
            rpbi.pClearValues = clear_values.data();

            frame_resource->commandbuffer->beginRenderPass(rpbi, vk::SubpassContents::eInline);

            std::vector<vk::Viewport> viewports;
            viewports.emplace_back(
                0,
                0,
                swapchain.extent.width,
                swapchain.extent.height,
                0,
                1.0f);

            frame_resource->commandbuffer->setViewport(0, viewports);

            std::vector<vk::Rect2D> scissors = { render_area };

            frame_resource->commandbuffer->setScissor(0, scissors);

            frame_resource->commandbuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.get());

            vk::Buffer vertex_buffers[] = { vertex_buffer.get_buffer() };
            vk::DeviceSize offsets[] = { 0 };
            frame_resource->commandbuffer->bindVertexBuffers(0, 1, vertex_buffers, offsets);
            frame_resource->commandbuffer->bindIndexBuffer(index_buffer.get_buffer(), 0, vk::IndexType::eUint32);

            model.draw(frame_resource->commandbuffer, pipeline_layout, desc_sets[virtual_frame_idx]);


            // Do the gui subpass
            frame_resource->commandbuffer->nextSubpass(vk::SubpassContents::eInline);
            gui.draw(virtual_frame_idx, frame_resource->commandbuffer);

            frame_resource->commandbuffer->endRenderPass();
            frame_resource->commandbuffer->end();

            vk::PipelineStageFlags stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            vk::SubmitInfo kernel{};
            kernel.waitSemaphoreCount = 1;
            kernel.pWaitSemaphores = &frame_resource->image_available.get();
            kernel.pWaitDstStageMask = &stage;
            kernel.commandBufferCount = 1;
            kernel.pCommandBuffers = &frame_resource->commandbuffer.get();
            kernel.signalSemaphoreCount = 1;
            kernel.pSignalSemaphores = &frame_resource->rendering_finished.get();
            graphics_queue.submit(kernel, frame_resource->fence.get());
        }

        // Present the frame
        vk::PresentInfoKHR present_i{};
        present_i.waitSemaphoreCount = 1;
        present_i.pWaitSemaphores = &frame_resource->rendering_finished.get();
        present_i.swapchainCount = 1;
        present_i.pSwapchains = &swapchain.handle.get();
        present_i.pImageIndices = &image_index;
        graphics_queue.presentKHR(present_i);

        virtual_frame_idx = (virtual_frame_idx + 1) % NUM_VIRTUAL_FRAME;
    }

    void Renderer::wait_idle()
    {
        vulkan.device->waitIdle();
    }
}    // namespace my_app
