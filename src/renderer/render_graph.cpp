#include "renderer/render_graph.hpp"
#include "imgui/imgui.h"
#include "renderer/hl_api.hpp"
#include <vulkan/vulkan_core.h>
#include "app.hpp"

namespace my_app
{

RenderGraph RenderGraph::create(vulkan::API &api)
{
    RenderGraph graph = {};
    graph.p_api = &api;
    graph.swapchain = graph.image_descs.add({.name = "Swapchain"});

    // how to clean this
    auto swapchain_h   = api.get_current_swapchain_h();
    auto &image        = graph.images[graph.swapchain];
    image.resolved_img = swapchain_h;
    image.resolved_rt  = api.create_rendertarget({.image_h = swapchain_h});

    return graph;
}

void RenderGraph::destroy()
{
    auto &api = *p_api;
    for (auto &[desc_h, image] : images)
    {
        if (image.resolved_img.is_valid()) {
            api.destroy_image(image.resolved_img);
        }
        if (image.resolved_rt.is_valid()) {
            api.destroy_rendertarget(image.resolved_rt);
        }
    }
}

void RenderGraph::on_resize(int /*width*/, int /*height*/)
{
    auto &api = *p_api;

    // how to clean this
    auto swapchain_h   = api.get_current_swapchain_h();
    auto &image        = images[swapchain];
    image.resolved_img = swapchain_h;
    api.destroy_rendertarget(image.resolved_rt);
    image.resolved_rt  = api.create_rendertarget({.image_h = swapchain_h});

    // resize all swapchain relative images
    for (auto &[desc_h, image] : images)
    {
        if (desc_h == swapchain) {
            continue;
        }
        if (!image.resolved_img.is_valid()) {
            continue;
        }

        auto &desc = *image_descs.get(desc_h);
        if (desc.size_type != SizeType::SwapchainRelative) {
            continue;
        }

        auto swapchain_extent = api.ctx.swapchain.extent;

        bool is_sampled_image          = !image.resource.sampled_images_in.empty();
        bool is_storage_image          = !image.resource.storage_images_in.empty();
        bool is_color_attachment       = !image.resource.color_attachment_in.empty();
        bool is_depth_attachment       = !image.resource.depth_attachment_in.empty();

        VkImageUsageFlags usage = 0;
        if (is_sampled_image) {
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

        vulkan::ImageInfo image_info = {
            .name          = desc.name.data(),
            .type          = desc.type,
            .format        = desc.format,
            .width         = static_cast<u32>(desc.size_type == SizeType::SwapchainRelative ? desc.size.x * swapchain_extent.width : desc.size.x),
            .height        = static_cast<u32>(desc.size_type == SizeType::SwapchainRelative ? desc.size.y * swapchain_extent.height : desc.size.y),
            .depth         = static_cast<u32>(desc.size.z),
            .usages        = usage,
        };

        std::cout << "Updating image " << desc.name << std::endl;
        api.destroy_image(image.resolved_img);
        image.resolved_img = api.create_image(image_info);

        if (is_color_attachment || is_depth_attachment) {
            std::cout << "Updating render target " << desc.name << std::endl;
            api.destroy_rendertarget(image.resolved_rt);
            image.resolved_rt = api.create_rendertarget({.image_h = image.resolved_img});
        }
    }
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

    for (auto sampled_desc : pass.sampled_images)
    {
        auto &image = images[sampled_desc];
        image.resource.sampled_images_in.push_back(pass_h);
    }

    for (auto storage_desc : pass.storage_images)
    {
        auto &image = images[storage_desc];
        image.resource.storage_images_in.push_back(pass_h);
    }
}


static void resolve_images(RenderGraph &graph)
{
    auto &api = *graph.p_api;

    // how to clean this
    auto swapchain_h = api.get_current_swapchain_h();
    auto &image = graph.images[graph.swapchain];
    image.resolved_img = swapchain_h;
    api.destroy_rendertarget(image.resolved_rt);
    image.resolved_rt = api.create_rendertarget({.image_h = swapchain_h});

    for (auto &[desc_h, image] : graph.images)
    {
        // is it needed to resolve twice?
        if (image.resolved_img.is_valid()) {
            continue;
        }

        // if external then resolved_img = external;
        // create rendertarget if resolved_img.usage & attachment

        auto &desc = *graph.image_descs.get(desc_h);

        auto swapchain_extent = api.ctx.swapchain.extent;

        bool is_sampled_image          = !image.resource.sampled_images_in.empty();
        bool is_storage_image          = !image.resource.storage_images_in.empty();
        bool is_color_attachment       = !image.resource.color_attachment_in.empty();
        bool is_depth_attachment       = !image.resource.depth_attachment_in.empty();

        VkImageUsageFlags usage = 0;
        if (is_sampled_image) {
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
            .extra_formats = desc.extra_formats,
            .width         = static_cast<u32>(desc.size_type == SizeType::SwapchainRelative ? desc.size.x * swapchain_extent.width : desc.size.x),
            .height        = static_cast<u32>(desc.size_type == SizeType::SwapchainRelative ? desc.size.y * swapchain_extent.height : desc.size.y),
            .depth         = static_cast<u32>(desc.size.z),
            .mip_levels    = desc.levels,
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

vulkan::ImageH RenderGraph::get_resolved_image(ImageDescH desc_h) const
{
    auto &image = images.at(desc_h);
    assert(image.resolved_img.is_valid());
    return image.resolved_img;
}

static void add_barriers(RenderGraph &graph, RenderPass &renderpass, std::vector<VkImageMemoryBarrier> &barriers, VkPipelineStageFlags &src_mask, VkPipelineStageFlags &dst_mask)
{
    auto &api = *graph.p_api;

    if (renderpass.color_attachment)
    {
        auto &image_resource = graph.images.at(*renderpass.color_attachment);
        auto &vk_image       = api.get_image(image_resource.resolved_img);

        auto src = vulkan::get_src_image_access(vk_image.usage);
        auto dst = vulkan::get_dst_image_access(vulkan::ImageUsage::ColorAttachment);
        src_mask |= src.stage;
        dst_mask |= dst.stage;

        barriers.emplace_back();
        auto &b        = barriers.back();
        b              = vulkan::get_image_barrier(vk_image, src, dst);
        vk_image.usage = vulkan::ImageUsage::ColorAttachment;
    }

    if (renderpass.depth_attachment)
    {
        auto &image_resource = graph.images.at(*renderpass.depth_attachment);
        auto &vk_image       = api.get_image(image_resource.resolved_img);

        auto src = vulkan::get_src_image_access(vk_image.usage);
        auto dst = vulkan::get_dst_image_access(vulkan::ImageUsage::DepthAttachment);
        src_mask |= src.stage;
        dst_mask |= dst.stage;

        barriers.emplace_back();
        auto &b        = barriers.back();
        b              = vulkan::get_image_barrier(vk_image, src, dst);
        vk_image.usage = vulkan::ImageUsage::DepthAttachment;
    }

    vulkan::ImageUsage dst_usage = renderpass.type == PassType::Graphics ? vulkan::ImageUsage::GraphicsShaderRead : vulkan::ImageUsage::ComputeShaderRead;
    for (auto sampled_image : renderpass.sampled_images)
    {
        auto &image_resource = graph.images.at(sampled_image);
        auto &vk_image       = api.get_image(image_resource.resolved_img);

        auto src = vulkan::get_src_image_access(vk_image.usage);
        auto dst = vulkan::get_dst_image_access(dst_usage);
        src_mask |= src.stage;
        dst_mask |= dst.stage;

        barriers.emplace_back();
        auto &b        = barriers.back();
        b              = vulkan::get_image_barrier(vk_image, src, dst);
        vk_image.usage = dst_usage;
    }

    for (auto external_image : renderpass.external_images)
    {
        auto &vk_image       = api.get_image(external_image);

        auto src = vulkan::get_src_image_access(vk_image.usage);
        auto dst = vulkan::get_dst_image_access(dst_usage);
        src_mask |= src.stage;
        dst_mask |= dst.stage;

        barriers.emplace_back();
        auto &b        = barriers.back();
        b              = vulkan::get_image_barrier(vk_image, src, dst);
        vk_image.usage = dst_usage;
    }


    dst_usage = renderpass.type == PassType::Graphics ? vulkan::ImageUsage::GraphicsShaderReadWrite : vulkan::ImageUsage::ComputeShaderReadWrite;
    for (auto storage_image : renderpass.storage_images)
    {
        auto &image_resource = graph.images.at(storage_image);
        auto &vk_image       = api.get_image(image_resource.resolved_img);

        auto src = vulkan::get_src_image_access(vk_image.usage);
        auto dst = vulkan::get_dst_image_access(dst_usage);
        src_mask |= src.stage;
        dst_mask |= dst.stage;

        barriers.emplace_back();
        auto &b        = barriers.back();
        b              = vulkan::get_image_barrier(vk_image, src, dst);
        vk_image.usage = dst_usage;
    }
}

static void flush_barriers(RenderGraph &graph, std::vector<VkImageMemoryBarrier> &barriers, VkPipelineStageFlags &src_mask, VkPipelineStageFlags &dst_mask)
{
    auto &api = *graph.p_api;
    auto cmd = api.ctx.frame_resources.get_current().command_buffer;
    vkCmdPipelineBarrier(cmd, src_mask, dst_mask, 0, 0, nullptr, 0, nullptr, barriers.size(), barriers.data());
}

void RenderGraph::execute()
{
    auto &api = *p_api;
    resolve_images(*this);

    for (auto &[renderpass_h, p_renderpass] : passes)
    {
        auto &renderpass = *p_renderpass;

        switch (renderpass.type)
        {
        case PassType::Graphics:
        {

            vulkan::PassInfo pass;

            // does the color attachment needs to be cleared?
            if (renderpass.color_attachment)
            {
                auto &color_resource = images.at(*renderpass.color_attachment);
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
                color_info.rt      = color_resource.resolved_rt;
                pass.color         = std::make_optional(color_info);
            }

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

            std::vector<VkImageMemoryBarrier> barriers;
            VkPipelineStageFlags src_mask = 0;
            VkPipelineStageFlags dst_mask = 0;
            add_barriers(*this, renderpass, barriers, src_mask, dst_mask);
            flush_barriers(*this, barriers, src_mask, dst_mask);

            usize viewport_width = 0;
            usize viewport_height = 0;

            auto swapchain_extent = api.ctx.swapchain.extent;
            if (renderpass.color_attachment) {
                auto &desc      = *image_descs.get(*renderpass.color_attachment);
                viewport_width  = static_cast<usize>(desc.size_type == SizeType::SwapchainRelative ? desc.size.x * swapchain_extent.width : desc.size.x);
                viewport_height = static_cast<usize>(desc.size_type == SizeType::SwapchainRelative ? desc.size.y * swapchain_extent.height : desc.size.y);
            }
            if (renderpass.depth_attachment) {
                auto &desc      = *image_descs.get(*renderpass.depth_attachment);
                viewport_width  = static_cast<usize>(desc.size_type == SizeType::SwapchainRelative ? desc.size.x * swapchain_extent.width : desc.size.x);
                viewport_height = static_cast<usize>(desc.size_type == SizeType::SwapchainRelative ? desc.size.y * swapchain_extent.height : desc.size.y);
            }

            api.begin_label(renderpass.name);
            api.begin_pass(std::move(pass));

            if (viewport_width && viewport_height) {
                api.set_viewport_and_scissor(viewport_width, viewport_height);
            }

            renderpass.exec(*this, renderpass, api);

            api.end_pass();
            api.end_label();
            break;
        }
        case PassType::Compute:
        {
            std::vector<VkImageMemoryBarrier> barriers;
            VkPipelineStageFlags src_mask = 0;
            VkPipelineStageFlags dst_mask = 0;
            add_barriers(*this, renderpass, barriers, src_mask, dst_mask);
            flush_barriers(*this, barriers, src_mask, dst_mask);

            api.begin_label(renderpass.name);

            renderpass.exec(*this, renderpass, api);

            api.end_label();
            break;
        }
        };
    }
}

void RenderGraph::display_ui(UI::Context &ui)
{
    auto &api = *p_api;
    if (ui.begin_window("Render Graph"))
    {
        for (const auto& [pass_h, p_pass] : passes)
        {
            auto &renderpass = *p_pass;
            if (ImGui::CollapsingHeader(renderpass.name.data()))
            {
                ImGui::Text("Type: %s", renderpass.type == PassType::Graphics ? "Graphics" : "Compute");
                ImGui::TextUnformatted("Inputs:");
                if (!renderpass.external_images.empty())
                {
                    ImGui::TextUnformatted(" - External images:");
                    for (const auto image_h : renderpass.external_images)
                    {
                        const auto &vkimage = api.get_image(image_h);
                        ImGui::Text("    - %s", vkimage.name);
                    }
                }
                if (!renderpass.sampled_images.empty())
                {
                    ImGui::TextUnformatted(" - Sampled images:");
                    for (const auto desc_h : renderpass.sampled_images)
                    {
                        const auto &desc = *image_descs.get(desc_h);
                        ImGui::Text("    - %s", desc.name.data());
                    }
                }

                ImGui::TextUnformatted("Outputs:");

                if (!renderpass.storage_images.empty())
                {
                    ImGui::TextUnformatted(" - Storage images:");
                    for (const auto desc_h : renderpass.sampled_images)
                    {
                        const auto &desc = *image_descs.get(desc_h);
                        ImGui::Text("    - %s", desc.name.data());
                    }
                }

                if (renderpass.color_attachment)
                {
                    const auto &desc = *image_descs.get(*renderpass.color_attachment);
                    ImGui::Text(" - Color attachment: %s", desc.name.data());
                }

                if (renderpass.depth_attachment)
                {
                    const auto &desc = *image_descs.get(*renderpass.depth_attachment);
                    ImGui::Text(" - Depth attachment: %s", desc.name.data());
                }
            }
        }

        ui.end_window();
    }
}
} // namespace my_app
