#include <vulkan/vulkan.hpp>
#include "renderer/hl_api.hpp"

namespace my_app::vulkan
{
    static RenderPass& find_or_create_render_pass(API& api, const PassInfo& info)
    {
	RenderPass rp;

	assert(info.clear);
	assert(info.present);
	assert(api.get_rendertarget(info.rt).is_swapchain);

	std::array<vk::AttachmentDescription, 1> attachments;

	attachments[0].format = api.ctx.swapchain.format.format;
	attachments[0].samples = vk::SampleCountFlagBits::e1;
	attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
	attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
	attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	attachments[0].initialLayout = vk::ImageLayout::eUndefined;
	attachments[0].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
	attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;
	attachments[0].flags = {};

	vk::AttachmentReference color_ref(0, vk::ImageLayout::eColorAttachmentOptimal);

	std::array<vk::SubpassDescription, 1> subpasses{};
	subpasses[0].flags = vk::SubpassDescriptionFlags(0);
	subpasses[0].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpasses[0].inputAttachmentCount = 0;
	subpasses[0].pInputAttachments = nullptr;
	subpasses[0].colorAttachmentCount = 1;
	subpasses[0].pColorAttachments = &color_ref;
	subpasses[0].pResolveAttachments = nullptr;
	subpasses[0].pDepthStencilAttachment = nullptr;
	subpasses[0].preserveAttachmentCount = 0;
	subpasses[0].pPreserveAttachments = nullptr;

	std::array<vk::SubpassDependency, 0> dependencies{};

	vk::RenderPassCreateInfo rp_info{};
	rp_info.attachmentCount = attachments.size();
	rp_info.pAttachments = attachments.data();
	rp_info.subpassCount = subpasses.size();
	rp_info.pSubpasses = subpasses.data();
	rp_info.dependencyCount = dependencies.size();
	rp_info.pDependencies = dependencies.data();
	rp.vkhandle = api.ctx.device->createRenderPassUnique(rp_info);


	api.renderpasses.push_back(std::move(rp));
	return api.renderpasses.back();
    }

    static FrameBuffer& find_or_create_frame_buffer(API& api, const PassInfo& info, const RenderPass& render_pass)
    {
	FrameBuffer fb;

	assert(api.get_rendertarget(info.rt).is_swapchain);

	std::array<vk::ImageView, 1> attachments = {
	    api.ctx.swapchain.get_current_image_view()
	};

	vk::FramebufferCreateInfo ci{};
	ci.renderPass = *render_pass.vkhandle;
	ci.attachmentCount = attachments.size();
	ci.pAttachments = attachments.data();
	ci.width = api.ctx.swapchain.extent.width;
	ci.height = api.ctx.swapchain.extent.height;
	ci.layers = 1;
	fb.vkhandle = api.ctx.device->createFramebufferUnique(ci);

	api.framebuffers.push_back(std::move(fb));
	return api.framebuffers.back();
    }

    void API::begin_pass(const PassInfo& info)
    {
	auto& render_pass = find_or_create_render_pass(*this, info);
	auto& frame_buffer = find_or_create_frame_buffer(*this, info, render_pass);

	auto& frame_resource = ctx.frame_resources.get_current();

	vk::Rect2D render_area{ vk::Offset2D(), ctx.swapchain.extent };

	std::array<vk::ClearValue, 3> clear_values;
	clear_values[0].color = vk::ClearColorValue(std::array<float, 4>{ 0.6f, 0.7f, 0.94f, 1.0f });
	clear_values[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);
	clear_values[2].color = vk::ClearColorValue(std::array<float, 4>{ 0.6f, 0.7f, 0.94f, 1.0f });

	vk::RenderPassBeginInfo rpbi{};
	rpbi.renderArea = render_area;
	rpbi.renderPass = *render_pass.vkhandle;
	rpbi.framebuffer = *frame_buffer.vkhandle;
	rpbi.clearValueCount = info.clear ? 1 : 0;
	rpbi.pClearValues = clear_values.data();

	frame_resource.commandbuffer->beginRenderPass(rpbi, vk::SubpassContents::eInline);
    }

    void API::end_pass()
    {
	auto& frame_resource = ctx.frame_resources.get_current();
	frame_resource.commandbuffer->endRenderPass();
    }
}
