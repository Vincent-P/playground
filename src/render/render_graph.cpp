#include "render/render_graph.hpp"

#include "app.hpp"
#include "render/hl_api.hpp"

#include <algorithm>
#include <cmath>
#include <fmt/core.h>
#include <imgui/imgui.h>
#include <vulkan/vulkan_core.h>

namespace my_app
{

void RenderGraph::create(RenderGraph &render_graph, vulkan::API &api)
{
    render_graph.p_api     = &api;
}

void RenderGraph::destroy()
{
    auto &api = *p_api;
    for (auto &[desc_h, image] : images)
    {
        if (image.resolved_img.is_valid())
        {
            api.destroy_image(image.resolved_img);
        }
    }
}

void RenderGraph::on_resize(int render_width, int render_height)
{
    this->render_width  = render_width;
    this->render_height = render_height;
}

void RenderGraph::start_frame()
{
    // api.start_frame() ?

    passes = {};
    images = {};
    image_descs = {};

    {
        auto &graph = *this;
        graph.swapchain = graph.image_descs.add({.name = "Swapchain"});
    }

    for (auto &[cache_info, cache_img, cache_used] : cache)
    {
        cache_used = false;
    }
}

void RenderGraph::add_pass(RenderPass &&_pass)
{
    auto pass_h = passes.add(std::move(_pass));
    auto &pass  = *passes.get(pass_h);

    for (auto color_desc : pass.color_attachments)
    {
        auto &image = images[color_desc];
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

    {
        // how to clean this
        auto swapchain_h   = api.get_current_swapchain_h();
        auto &image        = graph.images[graph.swapchain];
        image.resolved_img = swapchain_h;
    }

    for (auto &[desc_h, image] : graph.images)
    {
        // is it needed to resolve twice?
        if (image.resolved_img.is_valid())
        {
            assert(image.resolved_img == api.get_current_swapchain_h());
            continue;
        }

        // if external then resolved_img = external;
        // create rendertarget if resolved_img.usage & attachment

        auto &desc = *graph.image_descs.get(desc_h);

        bool is_sampled_image    = !image.resource.sampled_images_in.empty();
        bool is_storage_image    = !image.resource.storage_images_in.empty();
        bool is_color_attachment = !image.resource.color_attachment_in.empty();
        bool is_depth_attachment = !image.resource.depth_attachment_in.empty();

        VkImageUsageFlags usage = 0;
        if (is_sampled_image)
        {
            usage |= vulkan::sampled_image_usage;
        }
        if (is_storage_image)
        {
            usage |= vulkan::storage_image_usage;
        }
        if (is_color_attachment)
        {
            usage |= vulkan::color_attachment_usage;
        }
        if (is_depth_attachment)
        {
            usage |= vulkan::depth_attachment_usage;
        }

        // todo: samples, levels, layers
        auto size = graph.get_image_desc_size(desc);
        vulkan::ImageInfo image_info = {
            .name          = desc.name.data(),
            .type          = desc.type,
            .format        = desc.format,
            .extra_formats = desc.extra_formats,
            .width         = size.x,
            .height        = size.y,
            .depth         = static_cast<u32>(desc.size.z),
            .mip_levels    = desc.levels,
            .usages        = usage,
        };

        bool is_cached = false;

        for (auto &[cache_info, cache_img, cache_used] : graph.cache)
        {
            if (image_info.type == cache_info.type && image_info.format == cache_info.format
                && image_info.extra_formats == cache_info.extra_formats && image_info.width == cache_info.width
                && image_info.height == cache_info.height && image_info.depth == cache_info.depth
                && image_info.mip_levels == cache_info.mip_levels
                && image_info.generate_mip_levels == cache_info.generate_mip_levels
                && image_info.layers == cache_info.layers && image_info.samples == cache_info.samples
                && image_info.usages == cache_info.usages && image_info.memory_usage == cache_info.memory_usage
                && !cache_used)
            {
                image.resolved_img = cache_img;
                cache_used = true;
                is_cached = true;
                break;
            }
        }

        if (!is_cached)
        {
            fmt::print("Created new image for {}\n", desc.name);

            image.resolved_img = api.create_image(image_info);

            auto &vk_image = api.get_image(image.resolved_img);

            graph.cache.emplace_back(image_info, image.resolved_img, true);

            // clear new images to prevent sampling random things (happened with RADV)
            if ((vk_image.info.usages & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0
                && (vk_image.info.usages & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0)
            {
                VkClearColorValue color = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}};
                if (vulkan::is_uint(desc.format)) {
                    color.uint32[0] = 0u;
                    color.uint32[1] = 0u;
                    color.uint32[2] = 0u;
                    color.uint32[3] = 1u;
                }
                else if (vulkan::is_sint(desc.format)) {
                    color.int32[0] = 0;
                    color.int32[1] = 0;
                    color.int32[2] = 0;
                    color.int32[3] = 1;
                }
                fmt::print("Clearing {}\n", desc.name);
                api.clear_image(image.resolved_img, color);
            }
        }
    }
}

static void add_barriers(RenderGraph &graph, RenderPass &renderpass, std::vector<VkImageMemoryBarrier> &barriers,
                         VkPipelineStageFlags &src_mask, VkPipelineStageFlags &dst_mask)
{
    auto &api = *graph.p_api;

    const auto add_barrier = [&](auto image_h, auto desired_usage) {
        auto &vk_image = api.get_image(image_h);
        if (vulkan::is_image_barrier_needed(vk_image.usage, desired_usage))
        {
            auto src = vulkan::get_src_image_access(vk_image.usage);

            if (vk_image.usage == vulkan::ImageUsage::None)
            {
                fmt::print("WARNING: image ({}) usage is None!\n", vk_image.name);
            }

            auto dst = vulkan::get_dst_image_access(desired_usage);
            src_mask |= src.stage;
            dst_mask |= dst.stage;

            barriers.emplace_back();
            auto &b        = barriers.back();
            b              = vulkan::get_image_barrier(vk_image, src, dst);
            vk_image.usage = desired_usage;
        }
    };

    for (auto color_attachment : renderpass.color_attachments)
    {
        auto &image_resource = graph.images.at(color_attachment);
        add_barrier(image_resource.resolved_img, vulkan::ImageUsage::ColorAttachment);
    }

    if (renderpass.depth_attachment)
    {
        auto &image_resource = graph.images.at(*renderpass.depth_attachment);
        add_barrier(image_resource.resolved_img, vulkan::ImageUsage::DepthAttachment);
    }

    vulkan::ImageUsage dst_usage = renderpass.type == PassType::Graphics ? vulkan::ImageUsage::GraphicsShaderRead
                                                                         : vulkan::ImageUsage::ComputeShaderRead;

    for (auto sampled_image : renderpass.sampled_images)
    {
        auto &image_resource = graph.images.at(sampled_image);
        add_barrier(image_resource.resolved_img, dst_usage);
    }

    for (auto external_image : renderpass.external_images)
    {
        add_barrier(external_image, dst_usage);
    }

    dst_usage = renderpass.type == PassType::Graphics ? vulkan::ImageUsage::GraphicsShaderReadWrite
                                                      : vulkan::ImageUsage::ComputeShaderReadWrite;

    for (auto storage_image : renderpass.storage_images)
    {
        auto &image_resource = graph.images.at(storage_image);
        add_barrier(image_resource.resolved_img, dst_usage);
    }
}

static void flush_barriers(RenderGraph &graph, std::vector<VkImageMemoryBarrier> &barriers,
                           VkPipelineStageFlags &src_mask, VkPipelineStageFlags &dst_mask)
{
    auto &api           = *graph.p_api;
    VkCommandBuffer cmd = api.ctx.frame_resources.get_current().command_buffer;
    vkCmdPipelineBarrier(cmd, src_mask, dst_mask, 0, 0, nullptr, 0, nullptr, barriers.size(), barriers.data());
    api.barriers_this_frame += barriers.size();
}

bool RenderGraph::execute()
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
                for (auto color_attachment : renderpass.color_attachments)
                {
                    auto &color_resource = images.at(color_attachment);
                    usize color_pass_idx = 0; // index of the current pass in the color attachment's usage list
                    for (auto pass_h : color_resource.resource.color_attachment_in)
                    {
                        if (pass_h == renderpass_h)
                        {
                            break;
                        }
                        color_pass_idx++;
                    }
                    assert(color_pass_idx < color_resource.resource.color_attachment_in.size());

                    pass.colors.emplace_back();
                    auto &color_info = pass.colors.back();
                    // for now, clear whenever a render target is used for the first time in the frame and load
                    // otherwise
                    color_info.load_op = color_pass_idx == 0 ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
                    color_info.image_view = api.get_image(color_resource.resolved_img).default_view;
                }

                // same for depth
                if (renderpass.depth_attachment)
                {
                    auto &depth_resource = images.at(*renderpass.depth_attachment);
                    usize depth_pass_idx = 0; // index of the current pass in the depth attachment's usage list
                    for (auto pass_h : depth_resource.resource.depth_attachment_in)
                    {
                        if (pass_h == renderpass_h)
                        {
                            break;
                        }
                        depth_pass_idx++;
                    }
                    assert(depth_pass_idx < depth_resource.resource.depth_attachment_in.size());

                    vulkan::AttachmentInfo depth_info;
                    // for now, clear whenever a render target is used for the first time in the frame and load
                    // otherwise
                    depth_info.load_op = depth_pass_idx == 0 ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
                    depth_info.image_view = api.get_image(depth_resource.resolved_img).default_view;
                    pass.depth            = std::make_optional(depth_info);
                }

                std::vector<VkImageMemoryBarrier> barriers;
                VkPipelineStageFlags src_mask = 0;
                VkPipelineStageFlags dst_mask = 0;
                add_barriers(*this, renderpass, barriers, src_mask, dst_mask);
                flush_barriers(*this, barriers, src_mask, dst_mask);

                u32 viewport_width  = 1;
                u32 viewport_height = 1;

                for (auto color_attachment : renderpass.color_attachments)
                {
                    auto &desc = *image_descs.get(color_attachment);
                    auto size  = get_image_desc_size(desc);

                    viewport_width  = std::max(viewport_width, size.x);
                    viewport_height = std::max(viewport_height, size.y);
                }
                if (renderpass.depth_attachment)
                {
                    auto &desc      = *image_descs.get(*renderpass.depth_attachment);
                    auto size  = get_image_desc_size(desc);

                    viewport_width  = std::max(viewport_width, size.x);
                    viewport_height = std::max(viewport_height, size.y);
                }

                api.begin_label(renderpass.name);
                api.begin_pass(std::move(pass));

                if (viewport_width && viewport_height)
                {
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

    return true;
}

static void display_image(const vulkan::Image &vk_image)
{
  ImGui::Text("Size: %ux%u", vk_image.info.width, vk_image.info.height);
}

void RenderGraph::display_ui(UI::Context &ui)
{
    auto &api = *p_api;
    if (ui.begin_window("Render Graph", true))
    {
        if (ImGui::CollapsingHeader("Passes"))
        {
            for (const auto &[pass_h, p_pass] : passes)
            {
                auto &renderpass = *p_pass;
                if (ImGui::CollapsingHeader(renderpass.name.data()))
                {
                    if (renderpass.type == PassType::Graphics)
                    {
                        ImGui::TextUnformatted("Type: Graphics");
                    }
                    else if (renderpass.type == PassType::Compute)
                    {
                        ImGui::TextUnformatted("Type: Compute");
                    }
                    else
                    {
                        ImGui::TextUnformatted("Type: Unkown");
                    }

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

                    for (auto color_attachment : renderpass.color_attachments)
                    {
                        const auto &desc = *image_descs.get(color_attachment);
                        const auto &resolved = get_resolved_image(color_attachment);
                        const auto &image = api.get_image(resolved);
                        ImGui::Text(" - Color attachment: %s", desc.name.data());
                        ImGui::Text("Desc size: %0.fx%0.fx%0.f", desc.size.x, desc.size.y, desc.size.z);
                        display_image(image);
                    }

                    if (renderpass.depth_attachment)
                    {
                        const auto &desc = *image_descs.get(*renderpass.depth_attachment);
                        ImGui::Text(" - Depth attachment: %s", desc.name.data());
                    }
                }
            }
        }

        if (ImGui::CollapsingHeader("Resolved images"))
        {
            for (auto [desc_h, resource] : images)
            {
                    const auto &desc = *image_descs.get(desc_h);
                    const auto &resolved = get_resolved_image(desc_h);
                    ImGui::Text(" - %s: resolved %zu", desc.name.data(), resolved.hash());
            }
        }

        if (ImGui::CollapsingHeader("Cache"))
        {
            for (auto &[cache_info, cache_img, cache_used] : cache)
            {
                ImGui::Text(" - %zu: used %s", cache_img.hash(), cache_used ? "true" : "false");
            }
        }

        ui.end_window();
    }
}
} // namespace my_app
