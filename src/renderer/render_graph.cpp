#include "renderer/render_graph.hpp"
#include "renderer/hl_api.hpp"
#include <vulkan/vulkan_core.h>

namespace my_app
{

RenderGraph RenderGraph::create(vulkan::API &api)
{
    RenderGraph graph = {};

    graph.p_api = &api;

    graph.swapchain = graph.image_descs.add({});

    return graph;
}

void RenderGraph::destroy()
{
}

void RenderGraph::clear()
{
    passes = {};
    // images = {};
    // image_descs = {};

    for (auto &[desc_h, image] : images)
    {
        image.resource = {}; // clear "used_in" flags
    }
}

void RenderGraph::add_pass(RenderPass && _pass)
{
    auto pass_h = passes.add(std::move(_pass));
    auto& pass = *passes.get(pass_h);

    if (auto color_desc = pass.color_attachment)
    {
        auto &image = images[*color_desc];
        image.resource.color_attachment_in.push_back(pass_h);
    }

    if (auto depth_desc = pass.depth_attachment)
    {
        auto &image = images[*depth_desc];
        image.resource.depth_attachment_in.push_back(pass_h);
    }
}


static void resolve_images(RenderGraph &graph)
{
    auto &api = *graph.p_api;

    for (auto &[desc_h, image] : graph.images)
    {
        if (desc_h == graph.swapchain) {
            continue;
        }

        if (image.resolved_img.is_valid()) {
            continue;
        }

        auto &desc = *graph.image_descs.get(desc_h);

        auto swapchain_extent = api.ctx.swapchain.extent;

        bool is_sampled_image          = !image.resource.sampled_images_in.empty();
        bool is_combined_sampler_image = !image.resource.combined_sampler_images_in.empty();
        bool is_storage_image          = !image.resource.storage_images_in.empty();
        bool is_color_attachment       = !image.resource.color_attachment_in.empty();
        bool is_depth_attachment       = !image.resource.depth_attachment_in.empty();

        VkImageUsageFlags usage = 0;
        if (is_sampled_image || is_combined_sampler_image) {
            usage |= vulkan::sampled_image_usage;
        }
        if (is_storage_image) {
            usage |= vulkan::storage_image_usage;
        }
        if (is_color_attachment) {
            usage |= vulkan::color_attachment_usage;
        }
        if (is_depth_attachment) {
            usage |= vulkan::depth_attachment_usage;
        }

        // todo: samples, levels, layers
        vulkan::ImageInfo image_info = {
            .name          = desc.name.data(),
            .type          = desc.type,
            .format        = desc.format,
            .width         = static_cast<u32>(desc.size_type == SizeType::SwapchainRelative ? desc.size.x * swapchain_extent.width : desc.size.x),
            .height        = static_cast<u32>(desc.size_type == SizeType::SwapchainRelative ? desc.size.y * swapchain_extent.height : desc.size.y),
            .depth         = static_cast<u32>(desc.size.z),
            .usages        = usage,
        };

        std::cout << "Resolving image for " << desc.name << std::endl;
        image.resolved_img = api.create_image(image_info);

        if (is_color_attachment || is_depth_attachment) {
            std::cout << "Resolving render target for " << desc.name << std::endl;
            image.resolved_rt = api.create_rendertarget({.image_h = image.resolved_img});
        }
    }
}

void RenderGraph::execute()
{
    auto &api = *p_api;
    resolve_images(*this);

    for (auto &[renderpass_h, p_renderpass] : passes)
    {
        auto &renderpass = *p_renderpass;
        assert(renderpass.type == PassType::Graphics);
        assert(renderpass.color_attachment && *renderpass.color_attachment == swapchain);

        auto &color_resource = images.at(*renderpass.color_attachment);

        vulkan::PassInfo pass;
        pass.present = false;

        usize color_pass_idx = 0; // index of the current pass in the color attachment's usage list
        for (auto pass_h : color_resource.resource.color_attachment_in) {
            if (pass_h == renderpass_h) {
                break;
            }
            color_pass_idx++;
        }
        assert(color_pass_idx < color_resource.resource.color_attachment_in.size());

        vulkan::AttachmentInfo color_info;
        // for now, clear whenever a render target is used for the first time in the frame and load otherwise
        color_info.load_op = color_pass_idx == 0 ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        color_info.rt      = api.swapchain_rth;
        pass.color         = std::make_optional(color_info);

        // same for depth
        if (renderpass.depth_attachment)
        {
            auto &depth_resource = images.at(*renderpass.depth_attachment);
            usize depth_pass_idx = 0; // index of the current pass in the depth attachment's usage list
            for (auto pass_h : depth_resource.resource.depth_attachment_in) {
                if (pass_h == renderpass_h) {
                    break;
                }
                depth_pass_idx++;
            }
            assert(depth_pass_idx < depth_resource.resource.depth_attachment_in.size());

            vulkan::AttachmentInfo depth_info;
            // for now, clear whenever a render target is used for the first time in the frame and load otherwise
            depth_info.load_op = depth_pass_idx == 0 ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
            depth_info.rt      = depth_resource.resolved_rt;
            pass.depth         = std::make_optional(depth_info);
        }

        // barriers

        std::vector<VkImageMemoryBarrier> barriers;
        VkPipelineStageFlags src_mask = 0;
        VkPipelineStageFlags dst_mask = 0;

        if (renderpass.color_attachment)
        {
            if (*renderpass.color_attachment == swapchain)
            {
                VkImage vkimage = api.ctx.swapchain.get_current_image();
                VkImageSubresourceRange range{
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
                };
                auto &usage = api.swapchain_usages[api.ctx.swapchain.current_image];

                auto src = vulkan::get_src_image_access(usage);
                auto dst = vulkan::get_dst_image_access(vulkan::ImageUsage::ColorAttachment);
                src_mask |= src.stage;
                dst_mask |= dst.stage;

                barriers.emplace_back();
                auto &b = barriers.back();
                b              = vulkan::get_image_barrier(vkimage, src, dst, range);
                usage = vulkan::ImageUsage::ColorAttachment;
            }
            else
            {
                auto &image_resource = images.at(*renderpass.color_attachment);
                auto &vk_image = api.get_image(image_resource.resolved_img);

                auto src = vulkan::get_src_image_access(vk_image.usage);
                auto dst = vulkan::get_dst_image_access(vulkan::ImageUsage::ColorAttachment);
                src_mask |= src.stage;
                dst_mask |= dst.stage;

                barriers.emplace_back();
                auto &b = barriers.back();
                b              = vulkan::get_image_barrier(vk_image, src, dst);
                vk_image.usage = vulkan::ImageUsage::ColorAttachment;
            }
        }

        if (renderpass.depth_attachment)
        {
            auto &image_resource = images.at(*renderpass.depth_attachment);
            auto &vk_image = api.get_image(image_resource.resolved_img);

            auto src = vulkan::get_src_image_access(vk_image.usage);
            auto dst = vulkan::get_dst_image_access(vulkan::ImageUsage::DepthAttachment);
            src_mask |= src.stage;
            dst_mask |= dst.stage;

            barriers.emplace_back();
            auto &b = barriers.back();
            b              = vulkan::get_image_barrier(vk_image, src, dst);
            vk_image.usage = vulkan::ImageUsage::DepthAttachment;
        }

        auto cmd = api.ctx.frame_resources.get_current().command_buffer;
        vkCmdPipelineBarrier(cmd, src_mask, dst_mask, 0, 0, nullptr, 0, nullptr, barriers.size(), barriers.data());

        api.begin_label(renderpass.name);
        api.begin_pass(std::move(pass));

        renderpass.exec(*this, renderpass, api);

        api.end_pass();
        api.end_label();
    }
}
} // namespace my_app
