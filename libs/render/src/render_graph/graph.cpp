#include "render/render_graph/graph.h"

#include "render/vulkan/device.h"
#include "render/vulkan/framebuffer.h"
#include "render/vulkan/image.h"

#include <exo/profile.h>

void RenderGraph::execute(PassApi api, vulkan::WorkPool &work_pool)
{
	EXO_PROFILE_SCOPE;

	this->resources.begin_frame(api.device, this->i_frame);

	auto ctx = api.device.get_graphics_work(work_pool);
	ctx.begin();

	for (auto &pass : this->passes) {
		EXO_PROFILE_SCOPE_NAMED("render graph pass");
		switch (pass.type) {
		case PassType::Graphic: {
			auto &graphic_pass = pass.pass.graphic;
			auto  output_size  = this->resources.texture_desc_handle_size(graphic_pass.color_attachment);
			auto  output_image = this->resources.resolve_image(api.device, graphic_pass.color_attachment);

			auto framebuffer = this->resources.resolve_framebuffer(api.device,
				exo::Span{&graphic_pass.color_attachment, 1},
				graphic_pass.depth_attachment);

			ctx.barrier(output_image, vulkan::ImageUsage::ColorAttachment);

			exo::DynamicArray<vulkan::LoadOp, vulkan::MAX_ATTACHMENTS> load_ops;
			if (graphic_pass.clear) {
				load_ops.push_back(vulkan::LoadOp::clear({.color = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}}}));
			} else {
				load_ops.push_back(vulkan::LoadOp::ignore());
			}
			if (graphic_pass.depth_attachment.is_valid()) {
				load_ops.push_back(vulkan::LoadOp::clear({.color = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}}}));
			}
			ctx.begin_pass(framebuffer, load_ops);

			ctx.set_viewport(

				{.width = (float)output_size.x, .height = (float)output_size.y, .minDepth = 0.0f, .maxDepth = 1.0f});

			ctx.set_scissor({.extent = {.width = (u32)output_size.x, .height = (u32)output_size.y}});

			pass.execute(*this, api, ctx);

			ctx.end_pass();

			break;
		}
		case PassType::Raw: {
			pass.execute(*this, api, ctx);
			break;
		}
		}
	}
}

void RenderGraph::end_frame()
{
	this->resources.end_frame();
	this->passes.clear();
	this->i_frame += 1;
}

GraphicPass &RenderGraph::graphic_pass(
	Handle<TextureDesc> color_attachment, Handle<TextureDesc> depth_buffer, GraphicCb execute)
{
	passes.push(Pass::graphic(color_attachment, depth_buffer, std::move(execute)));
	return passes.last().pass.graphic;
}

RawPass &RenderGraph::raw_pass(RawCb execute)
{
	passes.push(Pass::raw(std::move(execute)));
	return passes.last().pass.raw;
}

Handle<TextureDesc> RenderGraph::output(TextureDesc desc) { return this->resources.texture_descs.add(std::move(desc)); }

int3 RenderGraph::image_size(Handle<TextureDesc> desc_handle)
{
	return int3(this->resources.texture_desc_handle_size(desc_handle), 1);
}
