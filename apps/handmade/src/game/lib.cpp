#include "common.h"
#include "platform.h"

#include "exo/logger.h"
#include "rhi/context.h"
#include "rhi/image.h"
#include "rhi/surface.h"
#include <cstdlib>
#include <vulkan/vulkan_core.h>

// -- Game

struct RenderState
{
	rhi::Context context;
	rhi::Surface surface;
};

struct GameState
{
	int counter;
	RenderState render;
};

void init_renderstate(Platform *platform, RenderState *render_state)
{
	render_state->context = rhi::Context::create(platform, {.enable_validation = true});
	render_state->surface =
		rhi::Surface::create(&render_state->context, platform->window->display_handle, platform->window->window_handle);
}

void shutdown_renderstate(Platform *platform, RenderState *render_state)
{
	render_state->context.wait_idle();
	render_state->surface.destroy(&render_state->context);
	render_state->context.destroy(platform);
}

void render(RenderState *render_state)
{
	rhi::Surface *surface = &render_state->surface;
	rhi::Context *render_ctx = &render_state->context;

	render_ctx->wait_idle();
	{
		auto i_frame = render_ctx->frame_count % rhi::FRAME_BUFFERING;

		if (!render_ctx->command_buffers[i_frame].is_empty()) {
			for (auto &is_used : render_ctx->command_buffers_is_used[i_frame]) {
				is_used = false;
			}

			// Maybe some command buffers were not used? :)
			render_ctx->vkdevice.FreeCommandBuffers(render_ctx->device,
				render_ctx->command_pools[i_frame],
				render_ctx->command_buffers[i_frame].len(),
				render_ctx->command_buffers[i_frame].data());
		}

		render_ctx->vkdevice.ResetCommandPool(render_ctx->device, render_ctx->command_pools[i_frame], 0);
	}

	bool outdated = render_ctx->acquire_next_backbuffer(surface);
	if (outdated) {
		surface->resize(render_ctx);
		return;
	}

	auto cmdbuffer = render_ctx->get_work();
	cmdbuffer.begin();

	cmdbuffer.wait_for_acquired(surface, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

	cmdbuffer.begin_debug_label("Super label");

	auto swapchain_image = surface->images[surface->current_image];

	cmdbuffer.barrier(swapchain_image, rhi::ImageUsage::TransferDst);

	VkClearColorValue clear_color = {};
	clear_color.float32[0] = 0.0f;
	clear_color.float32[1] = 1.0f;
	clear_color.float32[2] = 1.0f;
	clear_color.float32[3] = 1.0f;

	cmdbuffer.clear_image(swapchain_image, clear_color);

	cmdbuffer.end_debug_label();

	cmdbuffer.barrier(swapchain_image, rhi::ImageUsage::Present);

	cmdbuffer.prepare_present(surface);

	cmdbuffer.end();
	render_ctx->submit(&cmdbuffer);

	render_ctx->present(surface);
	render_ctx->frame_count += 1;
}

extern "C" __declspec(dllexport) void init(Platform *platform)
{
	auto *state = static_cast<GameState *>(calloc(1, sizeof(GameState)));
	init_renderstate(platform, &state->render);
	platform->game_state = state;
}

extern "C" __declspec(dllexport) void reload(Platform *platform)
{
	// do nothing
	(void)(platform);
	auto b = make_tester(98);
	b.bark();
}

extern "C" __declspec(dllexport) void update(Platform *platform)
{
	auto *state = platform->game_state;

	// Update
	state->counter += 1;
	state->counter += 3;
	state->counter -= 3;
	state->counter -= 2;
	state->counter += 2;

	char buffer[64] = {};
	snprintf(buffer, 64, "Value: %d\n", state->counter);
	platform->debug_print(buffer);

	// Render
	render(&state->render);
}

extern "C" __declspec(dllexport) void shutdown(Platform *platform)
{
	auto *state = platform->game_state;
	shutdown_renderstate(platform, &state->render);
	free(state);
}
