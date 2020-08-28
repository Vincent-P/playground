#include "renderer/render_graph.hpp"
#include "renderer/hl_api.hpp"

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
    images = {};
    image_descs = {};
}

void RenderGraph::add_pass(RenderPass && _pass)
{
    auto pass_h = passes.add(std::move(_pass));
    auto& pass = *passes.get(pass_h);

    if (auto color_desc = pass.color_attachment)
    {
        auto &image = images[*color_desc];
        image.resource.color_attachments_in.push_back(pass_h);
    }
}


static void resolve_images(RenderGraph &)
{
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

        auto &image_resource = images.at(*renderpass.color_attachment);

        usize color_pass_idx = 0; // index of the current pass in the color attachment's usage list
        for (auto pass_h : image_resource.resource.color_attachments_in) {
            if (pass_h == renderpass_h) {
                break;
            }
            color_pass_idx++;
        }
        assert(color_pass_idx < image_resource.resource.color_attachments_in.size());

        vulkan::PassInfo pass;
        pass.present = false;

        vulkan::AttachmentInfo color_info;
        // for now, clear whenever a render target is used for the first time in the frame and load otherwise
        color_info.load_op = color_pass_idx == 0 ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        color_info.rt      = api.swapchain_rth;
        pass.color         = std::make_optional(color_info);

        api.begin_label(renderpass.name);
        api.begin_pass(std::move(pass));

        renderpass.exec(*this, renderpass, api);

        api.end_pass();
        api.end_label();
    }
}
} // namespace my_app
