#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vk_mem_alloc.h>

#include "renderer.hpp"

namespace my_app
{
    Renderer::Renderer(GLFWwindow* window)
        : vk_ctx_(window)
        , Model_("models/Sponza/glTF/Sponza.gltf", vk_ctx_)
    {
        auto format = vk::Format::eA8B8G8R8UnormPack32;
        auto ci = vk::ImageCreateInfo();
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
        empty_image = Image{vk_ctx_.allocator, ci};

        // Create the sampler for the texture
        TextureSampler texture_sampler;
        vk::SamplerCreateInfo sci{};
        sci.magFilter = texture_sampler.magFilter;
        sci.minFilter = texture_sampler.minFilter;
        sci.mipmapMode = vk::SamplerMipmapMode::eLinear;
        sci.addressModeU = texture_sampler.addressModeU;
        sci.addressModeV = texture_sampler.addressModeV;
        sci.addressModeW = texture_sampler.addressModeW;
        sci.compareOp = vk::CompareOp::eNever;
        sci.borderColor = vk::BorderColor::eFloatOpaqueWhite;
        sci.maxAnisotropy = 1.0;
        sci.anisotropyEnable = VK_FALSE;
        sci.maxLod = 1.0f;
        empty_info.sampler = vk_ctx_.device->createSampler(sci);

        // Create the image view holding the texture
        vk::ImageViewCreateInfo vci{};
        vci.flags = {};
        vci.image = empty_image.GetImage();
        vci.format = format;
        vci.components = {vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA};
        vci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        vci.subresourceRange.baseMipLevel = 0;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.baseArrayLayer = 0;
        vci.subresourceRange.layerCount = 1;
        vci.viewType = vk::ImageViewType::e2D;
        empty_info.imageView = vk_ctx_.device->createImageView(vci);

        // wtf
        empty_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        // Create the swapchain
        CreateSwapchain();

        // Create ressources
        LoadShaders();
        CreateColorBuffer();
        CreateDepthBuffer();
        CreateUniformBuffer();
        CreateDescriptors();
        CreateVertexBuffer();
        CreateIndexBuffer();
        CreateFrameRessources();


        // Create the pipeline
        CreateRenderPass();
        CreateGraphicsPipeline();
    }

    Renderer::~Renderer()
    {
        // EMPTY TEXTURE

        vk_ctx_.device->destroy(empty_info.imageView);
        vk_ctx_.device->destroy(empty_info.sampler);
        empty_image.Free();

        // SWAPCHAIN OBJECTS
        DestroySwapchain();

        // PIPELINE OBJECTS

        vk_ctx_.device->destroy(scene_desc_layout);
        vk_ctx_.device->destroy(node_desc_layout);
        vk_ctx_.device->destroy(mat_desc_layout);
        vk_ctx_.device->destroy(desc_pool);

        vk_ctx_.device->destroy(vert_module);
        vk_ctx_.device->destroy(frag_module);

        vk_ctx_.device->destroy(pipeline_cache);
        vk_ctx_.device->destroy(pipeline_layout);
        vk_ctx_.device->destroy(pipeline);


        vertex_buffer.Free();
        index_buffer.Free();
        uniform_buffer.Free();
        Model_.Free();
    }

    void Renderer::DestroySwapchain()
    {
        for (auto& o : SwapChain_.ImageViews)
            vk_ctx_.device->destroy(o);

        vk_ctx_.device->destroy(depth_image_view);
        vk_ctx_.device->destroy(color_image_view);

        for (auto& frame_ressource : FrameRessources_)
            vk_ctx_.device->destroy(frame_ressource.Fence);

        depth_image.Free();
        color_image.Free();

        vk_ctx_.device->destroy(render_pass);

        for (auto& o : frame_buffers)
            vk_ctx_.device->destroy(o);
    }

    void Renderer::RecreateSwapchain()
    {
        vk_ctx_.device->waitIdle();
        DestroySwapchain();
        vk_ctx_.device->waitIdle();

        CreateSwapchain();
        CreateColorBuffer();
        CreateDepthBuffer();
        CreateRenderPass();
        CreateFrameRessources();
    }

    void Renderer::CreateSwapchain()
    {
        // Use default extent for the swapchain
        auto capabilities = vk_ctx_.physical_device.getSurfaceCapabilitiesKHR(*vk_ctx_.surface);
        SwapChain_.Extent = capabilities.currentExtent;

        // Find a good present mode (by priority Mailbox then Immediate then FIFO)
        auto present_modes = vk_ctx_.physical_device.getSurfacePresentModesKHR(*vk_ctx_.surface);
        SwapChain_.PresentMode = vk::PresentModeKHR::eFifo;

        for (auto& pm : present_modes)
            if (pm == vk::PresentModeKHR::eMailbox)
            {
                SwapChain_.PresentMode = vk::PresentModeKHR::eMailbox;
                break;
            }

        if (SwapChain_.PresentMode == vk::PresentModeKHR::eFifo)
            for (auto& pm : present_modes)
                if (pm == vk::PresentModeKHR::eImmediate)
                {
                    SwapChain_.PresentMode = vk::PresentModeKHR::eImmediate;
                    break;
                }

        // Find the best format
        auto formats = vk_ctx_.physical_device.getSurfaceFormatsKHR(*vk_ctx_.surface);
        SwapChain_.Format = formats[0];
        if (SwapChain_.Format.format == vk::Format::eUndefined)
        {
            SwapChain_.Format.format = vk::Format::eB8G8R8A8Unorm;
            SwapChain_.Format.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
        }
        else
        {
            for (const auto& f : formats)
                if (f.format == vk::Format::eB8G8R8A8Unorm && f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
                {
                    SwapChain_.Format = f;
                    break;
                }
        }

        assert(capabilities.maxImageCount >= NUM_VIRTUAL_FRAME);

        vk::SwapchainCreateInfoKHR ci{};
        ci.surface = *vk_ctx_.surface;
        ci.minImageCount = capabilities.minImageCount + 1;
        ci.imageFormat = SwapChain_.Format.format;
        ci.imageColorSpace = SwapChain_.Format.colorSpace;
        ci.imageExtent = SwapChain_.Extent;
        ci.imageArrayLayers = 1;
        ci.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;

        if (vk_ctx_.graphics_family_idx != vk_ctx_.present_family_idx)
        {
            uint32_t indices[] =
                {
                    (uint32_t)vk_ctx_.graphics_family_idx,
                    (uint32_t)vk_ctx_.present_family_idx

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
        ci.presentMode = SwapChain_.PresentMode;
        ci.clipped = VK_TRUE;

        SwapChain_.Handle = vk_ctx_.device->createSwapchainKHRUnique(ci);

        SwapChain_.Images = vk_ctx_.device->getSwapchainImagesKHR(*SwapChain_.Handle);

        SwapChain_.ImageViews.resize(SwapChain_.Images.size());

        for (size_t i = 0; i < SwapChain_.Images.size(); i++)
        {
            vk::ImageViewCreateInfo ci{};
            ci.image = SwapChain_.Images[i];
            ci.viewType = vk::ImageViewType::e2D;
            ci.format = SwapChain_.Format.format;
            ci.components = {vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA};
            ci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            ci.subresourceRange.baseMipLevel = 0;
            ci.subresourceRange.levelCount = 1;
            ci.subresourceRange.baseArrayLayer = 0;
            ci.subresourceRange.layerCount = 1;
            SwapChain_.ImageViews[i] = vk_ctx_.device->createImageView(ci);
        }
    }

    void Renderer::CreateColorBuffer()
    {
        vk::ImageCreateInfo ci{};
        ci.flags = {};
        ci.imageType = vk::ImageType::e2D;
        ci.format = SwapChain_.Format.format;
        ci.extent.width = SwapChain_.Extent.width;
        ci.extent.height = SwapChain_.Extent.height;
        ci.extent.depth = 1;
        ci.mipLevels = 1;
        ci.arrayLayers = 1;
        ci.samples = msaa_samples;
        ci.initialLayout = vk::ImageLayout::eUndefined;
        ci.usage = vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment;
        ci.queueFamilyIndexCount = 0;
        ci.pQueueFamilyIndices = NULL;
        ci.sharingMode = vk::SharingMode::eExclusive;

        color_image = Image(vk_ctx_.allocator, ci);

        vk::ImageViewCreateInfo vci{};
        vci.flags = {};
        vci.image = color_image.GetImage();
        vci.format = SwapChain_.Format.format;
        vci.components = {vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA};
        vci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        vci.subresourceRange.baseMipLevel = 0;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.baseArrayLayer = 0;
        vci.subresourceRange.layerCount = 1;
        vci.viewType = vk::ImageViewType::e2D;

        color_image_view = vk_ctx_.device->createImageView(vci);

        vk::ImageSubresourceRange subresource_range(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

        vk::ImageMemoryBarrier b{};
        b.oldLayout = vk::ImageLayout::eUndefined;
        b.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
        b.srcAccessMask = {};
        b.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
        b.image = color_image.GetImage();
        b.subresourceRange = subresource_range;

        vk_ctx_.TransitionLayout(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput, b);
    }


    void Renderer::CreateDepthBuffer()
    {
        std::vector<vk::Format> depthFormats =
        {
            vk::Format::eD32SfloatS8Uint, vk::Format::eD32Sfloat, vk::Format::eD24UnormS8Uint,
            vk::Format::eD16UnormS8Uint, vk::Format::eD16Unorm

        };

        for (auto& format : depthFormats)
        {
            auto depthFormatProperties = vk_ctx_.physical_device.getFormatProperties(format);
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
        ci.extent.width = SwapChain_.Extent.width;
        ci.extent.height = SwapChain_.Extent.height;
        ci.extent.depth = 1;
        ci.mipLevels = 1;
        ci.arrayLayers = 1;
        ci.samples = msaa_samples;
        ci.initialLayout = vk::ImageLayout::eUndefined;
        ci.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
        ci.queueFamilyIndexCount = 0;
        ci.pQueueFamilyIndices = NULL;
        ci.sharingMode = vk::SharingMode::eExclusive;

        depth_image = Image(vk_ctx_.allocator, ci);

        vk::ImageViewCreateInfo vci{};
        vci.flags = {};
        vci.image = depth_image.GetImage();
        vci.format = depth_format;
        vci.components = {vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA};
        vci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        vci.subresourceRange.baseMipLevel = 0;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.baseArrayLayer = 0;
        vci.subresourceRange.layerCount = 1;
        vci.viewType = vk::ImageViewType::e2D;

        depth_image_view = vk_ctx_.device->createImageView(vci);
    }

    void Renderer::CreateRenderPass()
    {
        std::array<vk::AttachmentDescription, 3> attachments;

        // Color attachment
        attachments[0].format = SwapChain_.Format.format;
        attachments[0].samples = msaa_samples;
        attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
        attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
        attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        attachments[0].initialLayout = vk::ImageLayout::eUndefined;
        attachments[0].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
        attachments[0].flags = {};

        // Depth attachment
        attachments[1].format = depth_format;
        attachments[1].samples = msaa_samples;
        attachments[1].loadOp = vk::AttachmentLoadOp::eClear;
        attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
        attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        attachments[1].initialLayout = vk::ImageLayout::eUndefined;
        attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        attachments[1].flags = vk::AttachmentDescriptionFlags();

        // Color resolve attachment
        attachments[2].format = SwapChain_.Format.format;
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

        vk::SubpassDescription subpass = {};
        subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpass.flags = vk::SubpassDescriptionFlags();
        subpass.inputAttachmentCount = 0;
        subpass.pInputAttachments = nullptr;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_ref;
        subpass.pResolveAttachments = &color_resolve_ref;
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
        render_pass = vk_ctx_.device->createRenderPass(rp_info);
    }

    void Renderer::CreateUniformBuffer()
    {
        uniform_buffer = Buffer(vk_ctx_.allocator, sizeof(MVP), vk::BufferUsageFlagBits::eUniformBuffer);
    }

    void Renderer::UpdateUniformBuffer(float time, Camera& camera)
    {
        // transformation, angle, rotations axis
        MVP ubo;
        ubo.cam_pos = camera.position;

        ubo.view = glm::lookAt(
            camera.position,                   // origin of camera
            camera.position + camera.front,    // where to look
            camera.up);

        ubo.proj = glm::infinitePerspective(
            glm::radians(55.0f),
            SwapChain_.Extent.width / (float)SwapChain_.Extent.height,
            .1f);

        // Vulkan clip space has inverted Y and half Z.
        ubo.clip = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f,
                             0.0f, -1.0f, 0.0f, 0.0f,
                             0.0f, 0.0f, 0.5f, 0.0f,
                             0.0f, 0.0f, 0.5f, 1.0f);

        void* mappedData = uniform_buffer.Map();
        memcpy(mappedData, &ubo, sizeof(ubo));
        uniform_buffer.Unmap();
    }

    void Renderer::CreateDescriptors()
    {
        std::vector<vk::DescriptorPoolSize> pool_sizes =
            {
                // TODO(vincent): count meshes
                vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, (4 + Model_.meshes.size()) * SwapChain_.Images.size()),
                // TODO(vincent): count textures
                vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 5 * Model_.materials.size() * SwapChain_.Images.size())

            };

        vk::DescriptorPoolCreateInfo dpci{};
        dpci.poolSizeCount = pool_sizes.size();
        dpci.pPoolSizes = pool_sizes.data();
        dpci.maxSets = (2 + Model_.meshes.size() + Model_.materials.size()) * SwapChain_.Images.size();
        desc_pool = vk_ctx_.device->createDescriptorPool(dpci);

        // Binding 0: Uniform buffer with scene informations (MVP)
        {
            std::vector<vk::DescriptorSetLayoutBinding> bindings =
                {
                    {0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr}

                };

            vk::DescriptorSetLayoutCreateInfo dslci{};
            dslci.bindingCount = bindings.size();
            dslci.pBindings = bindings.data();
            scene_desc_layout = vk_ctx_.device->createDescriptorSetLayout(dslci);

            std::vector<vk::DescriptorSetLayout> layouts{SwapChain_.Images.size(), scene_desc_layout};

            vk::DescriptorSetAllocateInfo dsai{};
            dsai.descriptorPool = desc_pool;
            dsai.pSetLayouts = layouts.data();
            dsai.descriptorSetCount = layouts.size();
            desc_sets = vk_ctx_.device->allocateDescriptorSets(dsai);

            for (size_t i = 0; i < SwapChain_.Images.size(); i++)
            {
                auto dbi = uniform_buffer.GetDescInfo();

                std::array<vk::WriteDescriptorSet, 1> writes;
                writes[0].dstSet = desc_sets[i];
                writes[0].descriptorCount = 1;
                writes[0].descriptorType = vk::DescriptorType::eUniformBuffer;
                writes[0].pBufferInfo = &dbi;
                writes[0].dstArrayElement = 0;
                writes[0].dstBinding = 0;

                vk_ctx_.device->updateDescriptorSets(writes, nullptr);
            }
        }

        // Binding 1: Material (samplers)
        {
            std::vector<vk::DescriptorSetLayoutBinding> bindings = {
                {0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr},
                {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr},
                {2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr},
                {3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr},
                {4, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr},

            };

            vk::DescriptorSetLayoutCreateInfo dslci{};
            dslci.bindingCount = bindings.size();
            dslci.pBindings = bindings.data();
            mat_desc_layout = vk_ctx_.device->createDescriptorSetLayout(dslci);

            // Per-Material descriptor sets
            for (auto& material : Model_.materials)
            {
                vk::DescriptorSetAllocateInfo dsai{};
                dsai.descriptorPool = desc_pool;
                dsai.pSetLayouts = &mat_desc_layout;
                dsai.descriptorSetCount = 1;
                material.desc_set = vk_ctx_.device->allocateDescriptorSets(dsai)[0];

                // Dont do this at home

                std::vector<vk::DescriptorImageInfo> imageDescriptors = {
                    empty_info,
                    empty_info,
                    material.normalTexture ? material.normalTexture->desc_info : empty_info,
                    material.occlusionTexture ? material.occlusionTexture->desc_info : empty_info,
                    material.emissiveTexture ? material.emissiveTexture->desc_info : empty_info};

                // TODO: glTF specs states that metallic roughness should be preferred, even if specular glosiness is present

                if (material.workflow == Material::PbrWorkflow::MetallicRoughness)
                {
                    if (material.baseColorTexture)
                        imageDescriptors[0] = material.baseColorTexture->desc_info;
                    if (material.metallicRoughnessTexture)
                        imageDescriptors[1] = material.metallicRoughnessTexture->desc_info;
                }
                else
                {
                    if (material.extension.diffuseTexture)
                        imageDescriptors[0] = material.extension.diffuseTexture->desc_info;
                    if (material.extension.specularGlossinessTexture)
                        imageDescriptors[1] = material.extension.specularGlossinessTexture->desc_info;
                }

                std::array<vk::WriteDescriptorSet, 5> writeDescriptorSets{};
                for (uint32_t i = 0; i < imageDescriptors.size(); i++)
                {
                    writeDescriptorSets[i].descriptorType = vk::DescriptorType::eCombinedImageSampler;
                    writeDescriptorSets[i].descriptorCount = 1;
                    writeDescriptorSets[i].dstSet = material.desc_set;
                    writeDescriptorSets[i].dstBinding = i;
                    writeDescriptorSets[i].pImageInfo = &imageDescriptors[i];
                }

                vk_ctx_.device->updateDescriptorSets(writeDescriptorSets, nullptr);
            }
        }

        // Binding 2: Nodes uniform (local transforms of each mesh)
        {
            std::vector<vk::DescriptorSetLayoutBinding> bindings =
                {
                    {0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr}
                };

            vk::DescriptorSetLayoutCreateInfo dslci{};
            dslci.bindingCount = bindings.size();
            dslci.pBindings = bindings.data();
            node_desc_layout = vk_ctx_.device->createDescriptorSetLayout(dslci);

            for (auto& node : Model_.scene_nodes)
                node.SetupNodeDescriptorSet(desc_pool, node_desc_layout, *vk_ctx_.device);
        }
    }

    void Renderer::CreateIndexBuffer()
    {
        auto size = Model_.indices.size() * sizeof(std::uint32_t);
        index_buffer = Buffer(vk_ctx_.allocator, size, vk::BufferUsageFlagBits::eIndexBuffer);

        void* mappedData = index_buffer.Map();
        memcpy(mappedData, Model_.indices.data(), size);
        index_buffer.Unmap();
    }

    void Renderer::CreateVertexBuffer()
    {
        auto size = Model_.vertices.size() * sizeof(Vertex);
        vertex_buffer = Buffer(vk_ctx_.allocator, size, vk::BufferUsageFlagBits::eVertexBuffer);

        void* mappedData = vertex_buffer.Map();
        memcpy(mappedData, Model_.vertices.data(), size);
        vertex_buffer.Unmap();
    }

    std::vector<char> readFile(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open())
            throw std::runtime_error("failed to open file!");


        size_t fileSize = (size_t)file.tellg();

        std::cout << "Opened file: " << filename << " (" << fileSize << ")\n";

        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }

    void Renderer::LoadShaders()
    {
        std::vector<char> vert_code = readFile("build/shaders/shader.frag.spv");
        std::vector<char> frag_code = readFile("build/shaders/shader.vert.spv");

        auto vert_i = vk::ShaderModuleCreateInfo();
        vert_i.codeSize = vert_code.size();
        vert_i.pCode = reinterpret_cast<const uint32_t*>(vert_code.data());
        vert_module = vk_ctx_.device->createShaderModule(vert_i);

        auto frag_i = vk::ShaderModuleCreateInfo();
        frag_i.codeSize = frag_code.size();
        frag_i.pCode = reinterpret_cast<const uint32_t*>(frag_code.data());
        frag_module = vk_ctx_.device->createShaderModule(frag_i);

        std::vector<vk::PipelineShaderStageCreateInfo> pipelineShaderStages =
            {
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
                    nullptr)

            };

        shader_stages = pipelineShaderStages;
    }

    void Renderer::CreateFrameRessources()
    {
        FrameRessources_.resize(NUM_VIRTUAL_FRAME);

        for (size_t i = 0; i < NUM_VIRTUAL_FRAME; i++)
        {
            auto frame_ressource = FrameRessources_.data() + i;

            vk::FenceCreateInfo fci{};
            frame_ressource->Fence = vk_ctx_.device->createFence({vk::FenceCreateFlagBits::eSignaled});
            frame_ressource->ImageAvailableSemaphore = vk_ctx_.device->createSemaphoreUnique({});
            frame_ressource->RenderingFinishedSemaphore = vk_ctx_.device->createSemaphoreUnique({});

            frame_ressource->CommandBuffer = std::move(vk_ctx_.device->allocateCommandBuffersUnique({vk_ctx_.command_pool, vk::CommandBufferLevel::ePrimary, 1})[0]);
        }
    }

    void Renderer::CreateGraphicsPipeline()
    {
        std::vector<vk::DynamicState> dynamic_states =
            {
                vk::DynamicState::eViewport,
                vk::DynamicState::eScissor

            };

        vk::PipelineDynamicStateCreateInfo dyn_i({}, dynamic_states.size(), dynamic_states.data());

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
        att_states[0].colorWriteMask = {vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};
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
        ms_i.rasterizationSamples = msaa_samples;
        ms_i.sampleShadingEnable = VK_TRUE;
        ms_i.alphaToCoverageEnable = VK_FALSE;
        ms_i.alphaToOneEnable = VK_FALSE;
        ms_i.minSampleShading = .2f;

        std::array<vk::DescriptorSetLayout, 3> layouts =
            {
                scene_desc_layout, mat_desc_layout, node_desc_layout

            };
        auto ci = vk::PipelineLayoutCreateInfo();
        ci.pSetLayouts = layouts.data();
        ci.setLayoutCount = layouts.size();

        auto pcr = vk::PushConstantRange(vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstBlockMaterial));
        ci.pushConstantRangeCount = 1;
        ci.pPushConstantRanges = &pcr;

        pipeline_layout = vk_ctx_.device->createPipelineLayout(ci);

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

        pipeline_cache = vk_ctx_.device->createPipelineCache(vk::PipelineCacheCreateInfo());
        pipeline = vk_ctx_.device->createGraphicsPipeline(pipeline_cache, pipe_i);
    }

    void Renderer::Resize(int, int)
    {
        RecreateSwapchain();
    }

    void Renderer::DrawFrame(double time, Camera& camera)
    {
        static uint32_t virtual_frame_idx = 0;

        auto& device = vk_ctx_.device;
        auto graphics_queue = device->getQueue(vk_ctx_.graphics_family_idx, 0);
        auto frame_ressource = FrameRessources_.data() + virtual_frame_idx;

        device->waitForFences(frame_ressource->Fence, VK_TRUE, UINT64_MAX);
        device->resetFences(frame_ressource->Fence);

        uint32_t image_index = 0;
        auto result = device->acquireNextImageKHR(*SwapChain_.Handle,
                                                  std::numeric_limits<uint64_t>::max(),
                                                  *frame_ressource->ImageAvailableSemaphore,
                                                  nullptr,
                                                  &image_index);

        if (result == vk::Result::eErrorOutOfDateKHR)
        {
            std::cerr << "The swapchain is out of date. Recreating it...\n";
            RecreateSwapchain();
            return;
        }

        // Create the framebuffer for the frame
        {
            std::array<vk::ImageView, 3> attachments = {
                color_image_view,
                depth_image_view,
                SwapChain_.ImageViews[image_index]
            };
            vk::FramebufferCreateInfo ci;
            ci.renderPass = render_pass;
            ci.attachmentCount = attachments.size();
            ci.pAttachments = attachments.data();
            ci.width = SwapChain_.Extent.width;
            ci.height = SwapChain_.Extent.height;
            ci.layers = 1;
            frame_ressource->FrameBuffer = vk_ctx_.device->createFramebufferUnique(ci);
        }

        // Update and Draw!!!
        {
            UpdateUniformBuffer(time, camera);

            auto renderArea = vk::Rect2D(vk::Offset2D(), SwapChain_.Extent);

            std::array<vk::ClearValue, 2> clearValues = {};
            clearValues[0].color = vk::ClearColorValue(std::array<float, 4>{0.5f, 0.5f, 0.5f, 1.0f});
            clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

            frame_ressource->CommandBuffer->begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

            frame_ressource->CommandBuffer->beginRenderPass(
                vk::RenderPassBeginInfo(
                    render_pass,
                    *frame_ressource->FrameBuffer,
                    renderArea,
                    clearValues.size(),
                    clearValues.data()),
                vk::SubpassContents::eInline);

            std::vector<vk::Viewport> viewports;
            viewports.emplace_back(
                0,
                0,
                SwapChain_.Extent.width,
                SwapChain_.Extent.height,
                0,
                1.0f);

            frame_ressource->CommandBuffer->setViewport(0, viewports);

            std::vector<vk::Rect2D> scissors = {renderArea};

            frame_ressource->CommandBuffer->setScissor(0, scissors);

            frame_ressource->CommandBuffer->bindPipeline(
                vk::PipelineBindPoint::eGraphics,
                pipeline);

            vk::Buffer vertexBuffers[] = {vertex_buffer.GetBuffer()};
            vk::DeviceSize offsets[] = {0};
            frame_ressource->CommandBuffer->bindVertexBuffers(0, 1, vertexBuffers, offsets);
            frame_ressource->CommandBuffer->bindIndexBuffer(index_buffer.GetBuffer(), 0, vk::IndexType::eUint32);

            Model_.draw(*frame_ressource->CommandBuffer, pipeline_layout, desc_sets[image_index]);

            frame_ressource->CommandBuffer->endRenderPass();
            frame_ressource->CommandBuffer->end();
        }

        // Submit the command buffer
        vk::PipelineStageFlags stages[] = {
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
        };

        auto kernel = vk::SubmitInfo();
        kernel.waitSemaphoreCount = 1;
        kernel.pWaitSemaphores = &frame_ressource->ImageAvailableSemaphore.get();
        kernel.pWaitDstStageMask = stages;
        kernel.commandBufferCount = 1;
        kernel.pCommandBuffers = &frame_ressource->CommandBuffer.get();
        kernel.signalSemaphoreCount = 1;
        kernel.pSignalSemaphores = &frame_ressource->RenderingFinishedSemaphore.get();
        graphics_queue.submit(kernel, frame_ressource->Fence);

        // Present the frame
        auto present_i = vk::PresentInfoKHR();
        present_i.waitSemaphoreCount = 1;
        present_i.pWaitSemaphores = &frame_ressource->RenderingFinishedSemaphore.get();
        present_i.swapchainCount = 1;
        present_i.pSwapchains = &SwapChain_.Handle.get();
        present_i.pImageIndices = &image_index;
        graphics_queue.presentKHR(present_i);

        virtual_frame_idx = (virtual_frame_idx + 1) % NUM_VIRTUAL_FRAME;
    }

    void Renderer::WaitIdle()
    {
        vk_ctx_.device->waitIdle();
    }
}    // namespace my_app
