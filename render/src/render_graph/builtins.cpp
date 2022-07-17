#include "render/render_graph/builtins.h"

#include "render/render_graph/graph.h"
#include "render/vulkan/commands.h"
#include "render/vulkan/device.h"
#include "render/vulkan/image.h"

namespace builtins
{
Handle<TextureDesc> acquire_next_image(RenderGraph &graph, SwapchainPass &pass)
{
	SwapchainPass *self   = &pass;
	auto           output = graph.output(TextureDesc{
				  .name = "swapchain desc",
				  .size = TextureSize::screen_relative(float2(1.0, 1.0)),
    });

	graph.raw_pass([self, output](RenderGraph &graph, PassApi &api, vulkan::ComputeWork &cmd) {
		auto is_outdated = api.device.acquire_next_swapchain(self->surface);
		while (is_outdated) {
			for (auto image : self->surface.images) {
				graph.resources.drop_image(image);
			}
			api.device.wait_idle();
			self->surface.recreate_swapchain(api.device);
			is_outdated = api.device.acquire_next_swapchain(self->surface);
		}

		graph.resources.screen_size = float2(self->surface.width, self->surface.height);
		graph.resources.set_image(output, self->surface.images[self->surface.current_image]);
		cmd.wait_for_acquired(self->surface, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR);
	});

	return output;
}

void present(RenderGraph &graph, SwapchainPass &pass, u64 signal_value)
{
	SwapchainPass *self = &pass;
	graph.raw_pass([self, signal_value](RenderGraph &graph, PassApi &api, vulkan::ComputeWork &cmd) {
		cmd.barrier(self->surface.images[self->surface.current_image], vulkan::ImageUsage::Present);
		cmd.end();
		cmd.prepare_present(self->surface);

		api.device.submit(cmd, std::span{&self->fence, 1}, std::span{&signal_value, 1});
		self->i_frame += 1;
		auto _is_outdated = api.device.present(self->surface, cmd);
	});
}

void copy_image(RenderGraph &graph, Handle<TextureDesc> src, Handle<TextureDesc> dst)
{
	ASSERT(src != dst);
	graph.raw_pass([src, dst](RenderGraph &graph, PassApi &api, vulkan::ComputeWork &cmd) {
		auto src_image = graph.resources.resolve_image(api.device, src);
		auto dst_image = graph.resources.resolve_image(api.device, dst);

		cmd.barrier(src_image, vulkan::ImageUsage::TransferSrc);
		cmd.barrier(dst_image, vulkan::ImageUsage::TransferDst);
		cmd.copy_image(src_image, dst_image);
	});
}

void blit_image(RenderGraph &graph, Handle<TextureDesc> src, Handle<TextureDesc> dst)
{
	ASSERT(src != dst);
	graph.raw_pass([src, dst](RenderGraph &graph, PassApi &api, vulkan::ComputeWork &cmd) {
		auto src_image = graph.resources.resolve_image(api.device, src);
		auto dst_image = graph.resources.resolve_image(api.device, dst);

		cmd.barrier(src_image, vulkan::ImageUsage::TransferSrc);
		cmd.barrier(dst_image, vulkan::ImageUsage::TransferDst);
		cmd.blit_image(src_image, dst_image);
	});
}
} // namespace builtins
