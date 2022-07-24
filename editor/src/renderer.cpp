#include "renderer.h"

#include "engine/render_world.h"

Renderer Renderer::create(u64 window_handle)
{
	Renderer renderer;
	renderer.base = SimpleRenderer::create(window_handle);
	return renderer;
}
void Renderer::draw(const RenderWorld &world)
{
	auto intermediate_buffer = base.render_graph.output(TextureDesc{
		.name = "render buffer desc",
		.size = TextureSize::screen_relative(float2(1.0, 1.0)),
	});
	base.render_graph.graphic_pass(intermediate_buffer,
		[](RenderGraph & /*graph*/, PassApi & /*api*/, vulkan::GraphicsWork & /*cmd*/) {});
	base.render(intermediate_buffer, 1.0);
}
