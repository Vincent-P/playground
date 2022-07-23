#include "render/render_graph/graph.h"

#include "render/vulkan/device.h"
#include "render/vulkan/framebuffer.h"
#include "render/vulkan/image.h"

void RenderGraph::execute(PassApi api, vulkan::WorkPool &work_pool)
{
	this->resources.begin_frame(api.device, this->i_frame);

	auto ctx = api.device.get_graphics_work(work_pool);
	ctx.begin();

	for (auto &pass : this->passes) {
		switch (pass.type) {
		case PassType::Graphic: {
			auto &graphic_pass = pass.pass.graphic;
			auto  output_size  = this->resources.texture_desc_handle_size(graphic_pass.color_attachment);
			auto  output_image = this->resources.resolve_image(api.device, graphic_pass.color_attachment);

			auto framebuffer = this->resources.resolve_framebuffer(api.device,
				std::span{&graphic_pass.color_attachment, 1},
				Handle<TextureDesc>::invalid());

			ctx.barrier(output_image, vulkan::ImageUsage::ColorAttachment);

			auto clear_color = vulkan::LoadOp::clear({.color = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}}});
			ctx.begin_pass(framebuffer, std::span{&clear_color, 1});

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

	this->resources.end_frame();
	this->passes.clear();
	this->i_frame += 1;
}

void RenderGraph::graphic_pass(Handle<TextureDesc> color_attachment, GraphicCb execute)
{
	passes.push_back(Pass::graphic(color_attachment, std::move(execute)));
}

void RenderGraph::raw_pass(RawCb execute) { passes.push_back(Pass::raw(std::move(execute))); }

Handle<TextureDesc> RenderGraph::output(TextureDesc desc) { return this->resources.texture_descs.add(std::move(desc)); }
int3                RenderGraph::image_size(Handle<TextureDesc> desc_handle)
{
	return int3(this->resources.texture_desc_handle_size(desc_handle), 1);
}
