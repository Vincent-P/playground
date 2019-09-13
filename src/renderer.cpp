#include "renderer.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <iostream>
#include <vk_mem_alloc.h>
#include <thsvs/thsvs_simpler_vulkan_synchronization.h>

#include "timer.hpp"
#include "tools.hpp"

namespace my_app
{
    Renderer::Renderer(GLFWwindow* window, const std::string& model_path)
        : vulkan(window)
        , voxelization(*this, 0)
        , voxel_visualization(*this, 1)
        , gui(*this, 2)
    {
        // Create the swapchain
        create_swapchain();

        // Create ressources
        create_frame_ressources();
        create_color_buffer();
        create_depth_buffer();

        // Create the pipeline
        create_render_pass();

        voxelization.init(model_path);
        voxel_visualization.init();
        gui.init();
    }

    Renderer::~Renderer()
    {
        // SWAPCHAIN OBJECTS
        destroy_swapchain();
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
                vulkan.graphics_family_idx,
                vulkan.present_family_idx
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
            vk::ImageViewCreateInfo ici{};
            ici.image = swapchain.images[i];
            ici.viewType = vk::ImageViewType::e2D;
            ici.format = swapchain.format.format;
            ici.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
            ici.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            ici.subresourceRange.baseMipLevel = 0;
            ici.subresourceRange.levelCount = 1;
            ici.subresourceRange.baseArrayLayer = 0;
            ici.subresourceRange.layerCount = 1;
            swapchain.image_views[i] = vulkan.device->createImageView(ici);
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
        ci.pQueueFamilyIndices = nullptr;
        ci.sharingMode = vk::SharingMode::eExclusive;

        color_image = Image{ "Color image", vulkan.allocator, ci };

        vk::ImageSubresourceRange subresource_range;
        subresource_range.aspectMask = vk::ImageAspectFlagBits::eColor;
        subresource_range.baseMipLevel = 0;
        subresource_range.levelCount = 1;
        subresource_range.baseArrayLayer = 0;
        subresource_range.layerCount = 1;

        vk::ImageViewCreateInfo vci{};
        vci.flags = {};
        vci.image = color_image.get_image();
        vci.format = swapchain.format.format;
        vci.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
        vci.subresourceRange = subresource_range;
        vci.viewType = vk::ImageViewType::e2D;

        color_image_view = vulkan.device->createImageView(vci);
        vulkan.transition_layout(color_image.get_image(), THSVS_ACCESS_NONE, THSVS_ACCESS_COLOR_ATTACHMENT_WRITE, subresource_range);
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
        ci.pQueueFamilyIndices = nullptr;
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
        attachments[2].loadOp = vk::AttachmentLoadOp::eClear;
        attachments[2].storeOp = vk::AttachmentStoreOp::eStore;
        attachments[2].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        attachments[2].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        attachments[2].initialLayout = vk::ImageLayout::eUndefined;
        attachments[2].finalLayout = vk::ImageLayout::ePresentSrcKHR;
        attachments[2].flags = {};

        vk::AttachmentReference color_ref(0, vk::ImageLayout::eColorAttachmentOptimal);
        vk::AttachmentReference depth_ref(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);
        vk::AttachmentReference color_resolve_ref(2, vk::ImageLayout::eColorAttachmentOptimal);

        // 3 subpasses: voxelization, voxel debug, gui
        std::array<vk::SubpassDescription, 3> subpasses{};
        subpasses[0].flags = vk::SubpassDescriptionFlags(0);
        subpasses[0].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpasses[0].inputAttachmentCount = 0;
        subpasses[0].pInputAttachments = nullptr;
        subpasses[0].colorAttachmentCount = 0;
        subpasses[0].pColorAttachments = nullptr;
        subpasses[0].pResolveAttachments = nullptr;
        subpasses[0].pDepthStencilAttachment = nullptr;
        subpasses[0].preserveAttachmentCount = 0;
        subpasses[0].pPreserveAttachments = nullptr;

        subpasses[1].flags = vk::SubpassDescriptionFlags(0);
        subpasses[1].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpasses[1].inputAttachmentCount = 0;
        subpasses[1].pInputAttachments = nullptr;
        subpasses[1].colorAttachmentCount = 1;
        subpasses[1].pColorAttachments = &color_ref;
        subpasses[1].pResolveAttachments = &color_resolve_ref;
        subpasses[1].pDepthStencilAttachment = &depth_ref;
        subpasses[1].preserveAttachmentCount = 0;
        subpasses[1].pPreserveAttachments = nullptr;

        subpasses[2].flags = vk::SubpassDescriptionFlags(0);
        subpasses[2].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpasses[2].inputAttachmentCount = 0;
        subpasses[2].pInputAttachments = nullptr;
        subpasses[2].colorAttachmentCount = 1;
        subpasses[2].pColorAttachments = &color_resolve_ref;
        subpasses[2].pResolveAttachments = nullptr;
        subpasses[2].pDepthStencilAttachment = nullptr;
        subpasses[2].preserveAttachmentCount = 0;
        subpasses[2].pPreserveAttachments = nullptr;

        vk::RenderPassCreateInfo rp_info{};
        rp_info.attachmentCount = attachments.size();
        rp_info.pAttachments = attachments.data();
        rp_info.subpassCount = subpasses.size();
        rp_info.pSubpasses = subpasses.data();
        rp_info.dependencyCount = 0;
        rp_info.pDependencies = nullptr;
        render_pass = vulkan.device->createRenderPassUnique(rp_info);
    }

    void Renderer::create_frame_ressources()
    {
        frame_resources.resize(NUM_VIRTUAL_FRAME);

        for (size_t i = 0; i < NUM_VIRTUAL_FRAME; i++)
        {
            auto frame_ressource = frame_resources.data() + i;

            vk::FenceCreateInfo fci{};
            frame_ressource->fence = vulkan.device->createFenceUnique({ vk::FenceCreateFlagBits::eSignaled });
            frame_ressource->image_available = vulkan.device->createSemaphoreUnique({});
            frame_ressource->rendering_finished = vulkan.device->createSemaphoreUnique({});

            frame_ressource->commandbuffer = std::move(vulkan.device->allocateCommandBuffersUnique({ vulkan.command_pool.get(), vk::CommandBufferLevel::ePrimary, 1 })[0]);
        }
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
            voxelization.update_uniform_buffer(virtual_frame_idx);
            voxel_visualization.update_uniform_buffer(virtual_frame_idx, camera);

            vk::Rect2D render_area{ vk::Offset2D(), swapchain.extent };

            std::array<vk::ClearValue, 3> clear_values;
            clear_values[0].color = vk::ClearColorValue(std::array<float, 4>{ 0.6f, 0.7f, 0.94f, 1.0f });
            clear_values[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);
            clear_values[2].color = vk::ClearColorValue(std::array<float, 4>{ 0.6f, 0.7f, 0.94f, 1.0f });

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

            voxelization.do_subpass(virtual_frame_idx, frame_resource->commandbuffer.get());

            frame_resource->commandbuffer->nextSubpass(vk::SubpassContents::eInline);
            auto& voxels_buffer = voxelization.get_voxels_buffer();
            voxel_visualization.do_subpass(virtual_frame_idx, frame_resource->commandbuffer.get(), voxels_buffer);

            frame_resource->commandbuffer->nextSubpass(vk::SubpassContents::eInline);
            gui.do_subpass(virtual_frame_idx, frame_resource->commandbuffer.get());

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
