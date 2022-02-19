#include <exo/prelude.h>

#include <exo/collections/dynamic_array.h>
#include <exo/collections/vector.h>
#include <exo/logger.h>
#include <exo/macros/packed.h>
#include <exo/memory/linear_allocator.h>
#include <exo/memory/scope_stack.h>
#include <exo/os/platform.h>
#include <exo/os/window.h>

#include <engine/render/base_renderer.h>
#include <engine/render/vulkan/context.h>
#include <engine/render/vulkan/device.h>
#include <engine/render/vulkan/surface.h>

#include <Tracy.hpp>
#include <windows.h>
namespace gfx = vulkan;

void *operator new(std::size_t count)
{
	auto ptr = malloc(count);
	TracyAlloc(ptr, count);
	return ptr;
}

void operator delete(void *ptr) noexcept
{
	TracyFree(ptr);
	free(ptr);
}

#define GPU_RENDER

u8  global_stack_mem[64 << 20];
int main(int /*argc*/, char ** /*argv*/)
{
	exo::LinearAllocator global_allocator = exo::LinearAllocator::with_external_memory(global_stack_mem, sizeof(global_stack_mem));
	exo::ScopeStack global_scope = exo::ScopeStack::with_allocator(&global_allocator);

	auto *platform = reinterpret_cast<exo::Platform *>(global_scope.allocate(exo::platform_get_size()));
	exo::platform_create(platform);

	auto *window = exo::Window::create(platform, global_scope, {1280, 720}, "Render sample");

#if defined(GPU_RENDER)
	auto *renderer = BaseRenderer::create(global_scope, window, {});

	exo::DynamicArray<Handle<gfx::Framebuffer>, gfx::MAX_SWAPCHAIN_IMAGES> framebuffers;
	framebuffers.resize(renderer->surface.images.size());

	for (usize i_image = 0; i_image < renderer->surface.images.size(); i_image += 1) {
		renderer->device.destroy_framebuffer(framebuffers[i_image]);
		framebuffers[i_image] = renderer->device.create_framebuffer(
			{
				.width  = renderer->surface.width,
				.height = renderer->surface.height,
			},
			std::array{renderer->surface.images[i_image]});
	}

	auto resize = [&]() {
		auto &device  = renderer->device;
		auto &surface = renderer->surface;

		device.wait_idle();
		surface.recreate_swapchain(device);

		for (usize i_image = 0; i_image < surface.images.size(); i_image += 1) {
			device.destroy_framebuffer(framebuffers[i_image]);
			framebuffers[i_image] = device.create_framebuffer(
				{
					.width  = surface.width,
					.height = surface.height,
				},
				std::array{surface.images[i_image]});
		}
	};

	auto draw = [&]() -> gfx::GraphicsWork {
		auto &device  = renderer->device;
		auto &surface = renderer->surface;

		auto             &work_pool = renderer->work_pools[renderer->frame_count % FRAME_QUEUE_LENGTH];
		gfx::GraphicsWork cmd       = device.get_graphics_work(work_pool);
		cmd.begin();

		cmd.wait_for_acquired(surface, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		auto framebuffer = framebuffers[surface.current_image];

		cmd.clear_barrier(surface.images[surface.current_image], gfx::ImageUsage::ColorAttachment);
		cmd.begin_pass(framebuffer, std::array{gfx::LoadOp::clear({.color = {.float32 = {1.0f, 1.0f, 1.0f, 1.0f}}})});
		cmd.end_pass();
		cmd.barrier(surface.images[surface.current_image], gfx::ImageUsage::Present);
		cmd.end();

		return cmd;
	};
#else
    int X = 0;
#endif

	while (!window->should_close()) {
		window->poll_events();

		bool has_resize = false;
		for (const auto &event : window->events) {
			switch (event.type) {
			case exo::Event::KeyType: {
				break;
			}
			case exo::Event::CharacterType: {
				break;
			}
			case exo::Event::ResizeType: {
				has_resize = true;
				break;
			}
			default:
				break;
			}
		}
		window->events.clear();

		// -- Render
#if defined(GPU_RENDER)
		if (has_resize)
		{
			resize();
		}

		bool out_of_date_swapchain = renderer->start_frame();
		while (out_of_date_swapchain)
		{
			resize();
			out_of_date_swapchain = renderer->start_frame();
		}

		auto cmd = draw();
		out_of_date_swapchain = renderer->end_frame(cmd);
#else
		int  MidPoint = (X++ % (64 * 1024)) / 64;
		HWND Window   = (HWND)window->get_win32_hwnd();
		RECT Client;
		GetClientRect(Window, &Client);
		HDC DC = GetDC(Window);

		PatBlt(DC, 0, 0, MidPoint, Client.bottom, BLACKNESS);
		if (Client.right > MidPoint) {
			PatBlt(DC, MidPoint, 0, Client.right - MidPoint, Client.bottom, WHITENESS);
		}
		ReleaseDC(Window, DC);
#endif

		FrameMark
	}

	exo::platform_destroy(platform);
	return 0;
}
