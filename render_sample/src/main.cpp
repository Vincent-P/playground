#include <cross/platform.h>
#include <cross/window.h>
#include <exo/memory/linear_allocator.h>
#include <exo/memory/scope_stack.h>

#include <render/simple_renderer.h>
#include <render/vulkan/commands.h>

#define GPU_RENDER

#include <Tracy.hpp>
#include <cstdio>
#if !defined(GPU_RENDER)
#include <windows.h>
#endif

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

u8  global_stack_mem[64 << 20];
int main(int /*argc*/, char ** /*argv*/)
{
	exo::LinearAllocator global_allocator =
		exo::LinearAllocator::with_external_memory(global_stack_mem, sizeof(global_stack_mem));
	exo::ScopeStack global_scope = exo::ScopeStack::with_allocator(&global_allocator);

	auto *platform = reinterpret_cast<cross::platform::Platform *>(global_scope.allocate(cross::platform::get_size()));
	cross::platform::create(platform);

	auto *window = cross::Window::create(global_scope, {1280, 720}, "Render sample");

#if defined(GPU_RENDER)
	auto renderer = SimpleRenderer::create(window->get_win32_hwnd());
#else
	int X = 0;
#endif
	int i_frame = 0;

	while (!window->should_close()) {
		window->poll_events();
		window->events.clear();

		// -- Render
		printf("%d\n", i_frame);
#if defined(GPU_RENDER)
		{
			auto intermediate_buffer = renderer.render_graph.output(TextureDesc{
				.name = "render buffer desc",
				.size = TextureSize::screen_relative(float2(1.0, 1.0)),
			});
			renderer.render_graph.graphic_pass(intermediate_buffer,
				Handle<TextureDesc>::invalid(),
				[](RenderGraph & /*graph*/, PassApi & /*api*/, vulkan::GraphicsWork & /*cmd*/) {});
			renderer.render(intermediate_buffer, 1.0);
		}
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
		i_frame += 1;
		FrameMark
	}

	cross::platform::destroy();
	return 0;
}
