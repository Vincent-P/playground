#include "renderer.h"

#include "exo/macros/packed.h"

#include "exo/profile.h"
#include "render/bindings.h"
#include "render/shader_watcher.h"
#include "render/vulkan/device.h"
#include "render/vulkan/image.h"

Renderer Renderer::create(u64 display_handle, u64 window_handle, AssetManager *asset_manager)
{
	Renderer renderer;
    renderer.base          = SimpleRenderer::create(display_handle, window_handle);
	renderer.asset_manager = asset_manager;
	renderer.mesh_renderer = MeshRenderer::create(renderer.base.device);
	renderer.ui_renderer   = UiRenderer::create(renderer.base.device, int2(1024, 1024));
	WATCH_LIB_SHADER(renderer.base.shader_watcher);

	renderer.srgb_pass.program = renderer.base.device.create_program("srgb pass",
		vulkan::ComputeState{
			.shader = renderer.base.device.create_shader(SHADER_PATH("srgb_pass.comp.glsl.spv")),
		});

	return renderer;
}

static void register_srgb_pass(Renderer &renderer, Handle<TextureDesc> input, Handle<TextureDesc> output)
{
	auto &graph = renderer.base.render_graph;

	auto compute_program = renderer.srgb_pass.program;

	graph.raw_pass([compute_program, input, output](RenderGraph &graph, PassApi &api, vulkan::ComputeWork &cmd) {
		PACKED(struct Options {
			u32 linear_input_buffer_texture;
			u32 srgb_output_buffer_image;
			u32 pad00;
			u32 pad01;
		})

		auto input_image  = graph.resources.resolve_image(api.device, input);
		auto output_image = graph.resources.resolve_image(api.device, output);

		ASSERT(graph.image_size(input) == graph.image_size(output));
		auto dispatch_size = exo::uint3(graph.image_size(input));
		dispatch_size.x    = (dispatch_size.x / 16) + (dispatch_size.x % 16 != 0);
		dispatch_size.y    = (dispatch_size.y / 16) + (dispatch_size.y % 16 != 0);

		auto options = bindings::bind_option_struct<Options>(api.device, api.uniform_buffer, cmd);
		options[0].linear_input_buffer_texture = api.device.get_image_sampled_index(input_image);
		options[0].srgb_output_buffer_image    = api.device.get_image_storage_index(output_image);

		cmd.barrier(input_image, vulkan::ImageUsage::ComputeShaderRead);
		cmd.barrier(output_image, vulkan::ImageUsage::ComputeShaderReadWrite);
		cmd.bind_pipeline(compute_program);
		cmd.dispatch(dispatch_size);
	});
}

DrawResult Renderer::draw(DrawInput input)
{
	EXO_PROFILE_SCOPE;
	base.start_frame();

	if (input.world) {
		register_upload_nodes(this->base.render_graph,
			this->mesh_renderer,
			this->base.device,
			this->base.upload_buffer,
			this->asset_manager,
			*input.world);
	}

	auto       scene_rt           = Handle<TextureDesc>::invalid();
	const bool has_world_viewport = input.world_viewport_size.x > 0 && input.world_viewport_size.y > 0;
	if (has_world_viewport) {

		scene_rt = base.render_graph.output(TextureDesc{
			.name = "world viewport",
			.size = TextureSize::absolute(exo::int2(input.world_viewport_size)),
		});

		register_graphics_nodes(this->base.render_graph, this->mesh_renderer, scene_rt);
	}

	auto screen_rt = base.render_graph.output(TextureDesc{
		.name = "screen rt",
		.size = TextureSize::screen_relative(float2(1.0, 1.0)),
	});

	if (input.painter) {
		register_graph(this->base.render_graph, this->ui_renderer, input.painter, screen_rt);
	}

	auto srgb_screen_rt = base.render_graph.output(TextureDesc{
		.name = "srgb screen rt",
		.size = TextureSize::screen_relative(float2(1.0, 1.0)),
	});

	register_srgb_pass(*this, screen_rt, srgb_screen_rt);
	base.render(srgb_screen_rt, 1.0);

	// Prepare the result
	DrawResult draw_result = {};

	draw_result.glyph_atlas_index = this->base.device.get_image_sampled_index(this->ui_renderer.glyph_atlas);

	if (scene_rt.is_valid()) {
		auto scene_rt_handle             = this->base.render_graph.resources.resolve_image(this->base.device, scene_rt);
		draw_result.scene_viewport_index = this->base.device.get_image_sampled_index(scene_rt_handle);
	}

	base.end_frame();

	return draw_result;
}
