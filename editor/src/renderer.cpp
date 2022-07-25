#include "renderer.h"

#include <render/shader_watcher.h>
#include <render/vulkan/device.h>

Renderer Renderer::create(u64 window_handle, AssetManager *asset_manager)
{
	Renderer renderer;
	renderer.base          = SimpleRenderer::create(window_handle);
	renderer.asset_manager = asset_manager;
	renderer.mesh_renderer = MeshRenderer::create(renderer.base.device);
	renderer.ui_renderer   = UiRenderer::create(renderer.base.device, int2(1024, 1024));
	WATCH_LIB_SHADER(renderer.base.shader_watcher);

	return renderer;
}

void Renderer::draw(const RenderWorld &world, Painter *painter)
{
	base.start_frame();

	register_upload_nodes(this->base.render_graph,
		this->mesh_renderer,
		this->base.device,
		this->base.upload_buffer,
		this->asset_manager,
		world);

	auto intermediate_buffer = base.render_graph.output(TextureDesc{
		.name = "render buffer desc",
		.size = TextureSize::screen_relative(float2(1.0, 1.0)),
	});
	register_graphics_nodes(this->base.render_graph, this->mesh_renderer, intermediate_buffer);
	if (painter) {
		auto &pass = register_graph(this->base.render_graph, this->ui_renderer, painter, intermediate_buffer);
		pass.clear = false;
	}
	base.render(intermediate_buffer, 1.0);
}

u32 Renderer::glyph_atlas_index() const
{
	return this->base.device.get_image_sampled_index(this->ui_renderer.glyph_atlas);
}
