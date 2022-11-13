#include "render/vulkan/framebuffer.h"

#include "render/vulkan/device.h"
#include "render/vulkan/utils.h"

#include <algorithm>

namespace vulkan
{
RenderPass create_renderpass(Device &device, const FramebufferFormat &format, exo::Span<const LoadOp> load_ops)
{
	auto attachments_count = format.attachments_format.size() + (format.depth_format.has_value() ? 1 : 0);
	ASSERT(load_ops.len() == attachments_count);

	exo::DynamicArray<VkAttachmentReference, MAX_ATTACHMENTS>   color_refs;
	exo::DynamicArray<VkAttachmentDescription, MAX_ATTACHMENTS> attachment_descriptions;

	for (u32 i_color = 0; i_color < format.attachments_format.size(); i_color += 1) {
		color_refs.push_back({
			.attachment = static_cast<u32>(attachment_descriptions.size()),
			.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		});

		attachment_descriptions.push_back(VkAttachmentDescription{
			.format        = format.attachments_format[i_color],
			.samples       = VK_SAMPLE_COUNT_1_BIT,
			.loadOp        = to_vk(load_ops[i_color]),
			.storeOp       = VK_ATTACHMENT_STORE_OP_STORE,
			.initialLayout = load_ops[i_color].type == LoadOp::Type::Clear ? VK_IMAGE_LAYOUT_UNDEFINED
		                                                                   : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.finalLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		});
	}

	VkAttachmentReference depth_ref{
		.attachment = 0,
		.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
	};

	if (format.depth_format.has_value()) {
		depth_ref.attachment = static_cast<u32>(attachment_descriptions.size());

		attachment_descriptions.push_back(VkAttachmentDescription{
			.format        = format.depth_format.value(),
			.samples       = VK_SAMPLE_COUNT_1_BIT,
			.loadOp        = to_vk(load_ops.back()),
			.storeOp       = VK_ATTACHMENT_STORE_OP_STORE,
			.initialLayout = load_ops.back().type == LoadOp::Type::Clear ? VK_IMAGE_LAYOUT_UNDEFINED
		                                                                 : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.finalLayout   = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		});
	}

	VkSubpassDescription subpass;
	subpass.flags                   = 0;
	subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.inputAttachmentCount    = 0;
	subpass.pInputAttachments       = nullptr;
	subpass.colorAttachmentCount    = static_cast<u32>(color_refs.size());
	subpass.pColorAttachments       = color_refs.data();
	subpass.pResolveAttachments     = nullptr;
	subpass.pDepthStencilAttachment = format.depth_format.has_value() ? &depth_ref : nullptr;
	subpass.preserveAttachmentCount = 0;
	subpass.pPreserveAttachments    = nullptr;

	VkRenderPassCreateInfo rp_info = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
	rp_info.attachmentCount        = static_cast<u32>(attachment_descriptions.size());
	rp_info.pAttachments           = attachment_descriptions.data();
	rp_info.subpassCount           = 1;
	rp_info.pSubpasses             = &subpass;
	rp_info.dependencyCount        = 0;
	rp_info.pDependencies          = nullptr;

	VkRenderPass vk_renderpass = VK_NULL_HANDLE;
	vk_check(vkCreateRenderPass(device.device, &rp_info, nullptr, &vk_renderpass));

	return {.vkhandle = vk_renderpass, .load_ops = load_ops};
}

RenderPass &Device::find_or_create_renderpass(Framebuffer &framebuffer, exo::Span<const LoadOp> load_ops)
{
	ASSERT(framebuffer.color_attachments.size() == framebuffer.format.attachments_format.size());
	ASSERT(framebuffer.depth_attachment.is_valid() == framebuffer.format.depth_format.has_value());

	for (auto &renderpass : framebuffer.renderpasses) {
		if (std::ranges::equal(renderpass.load_ops, load_ops)) {
			return renderpass;
		}
	}

	framebuffer.renderpasses.push_back(create_renderpass(*this, framebuffer.format, load_ops));
	return framebuffer.renderpasses.back();
}

Handle<Framebuffer> Device::create_framebuffer(
	int3 size, exo::Span<const Handle<Image>> color_attachments, Handle<Image> depth_attachment)
{
	Framebuffer fb       = {};
	fb.format            = {.size = size};
	fb.color_attachments = color_attachments;
	fb.depth_attachment  = depth_attachment;

	auto attachments_count = fb.color_attachments.size() + (fb.depth_attachment.is_valid() ? 1 : 0);

	exo::DynamicArray<VkImageView, MAX_ATTACHMENTS> attachment_views;
	for (const auto &attachment : fb.color_attachments) {
		const auto &image = this->images.get(attachment);
		attachment_views.push_back(image.full_view.vkhandle);
		fb.format.attachments_format.push_back(image.desc.format);
	}
	if (fb.depth_attachment.is_valid()) {
		const auto &image = this->images.get(fb.depth_attachment);
		attachment_views.push_back(image.full_view.vkhandle);
		fb.format.depth_format = image.desc.format;
	}

	exo::DynamicArray<LoadOp, MAX_ATTACHMENTS> load_ops;
	for (usize i_attachment = 0; i_attachment < attachments_count; ++i_attachment) {
		load_ops.push_back(LoadOp::ignore());
	}
	auto &renderpass = this->find_or_create_renderpass(fb, load_ops);

	VkFramebufferCreateInfo fb_info = {.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
	fb_info.renderPass              = renderpass.vkhandle;
	fb_info.attachmentCount         = static_cast<u32>(attachments_count);
	fb_info.pAttachments            = attachment_views.data();
	fb_info.width                   = static_cast<u32>(fb.format.size[0]);
	fb_info.height                  = static_cast<u32>(fb.format.size[1]);
	fb_info.layers                  = static_cast<u32>(fb.format.size[2]);

	fb.vkhandle = VK_NULL_HANDLE;
	vk_check(vkCreateFramebuffer(device, &fb_info, nullptr, &fb.vkhandle));

	return framebuffers.add(std::move(fb));
}

void Device::destroy_framebuffer(Handle<Framebuffer> framebuffer_handle)
{
	auto &framebuffer = framebuffers.get(framebuffer_handle);
	vkDestroyFramebuffer(device, framebuffer.vkhandle, nullptr);

	for (auto &renderpass : framebuffer.renderpasses) {
		vkDestroyRenderPass(device, renderpass.vkhandle, nullptr);
	}
	framebuffers.remove(framebuffer_handle);
}
} // namespace vulkan
