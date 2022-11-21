#include "ui_renderer/ui_renderer.h"

#include "painter/painter.h"
#include "render/bindings.h"
#include "render/render_graph/graph.h"
#include "render/shader_watcher.h"
#include "render/vulkan/device.h"
#include "render/vulkan/image.h"
#include "render/vulkan/pipelines.h"

UiRenderer UiRenderer::create(vulkan::Device &device, int2 atlas_resolution)
{
	UiRenderer renderer = {};

	vulkan::GraphicsState gui_state = {};
	gui_state.vertex_shader         = device.create_shader(SHADER_PATH("ui.vert.glsl.spv"));
	gui_state.fragment_shader       = device.create_shader(SHADER_PATH("ui.frag.glsl.spv"));
	gui_state.attachments_format    = {.attachments_format = {VK_FORMAT_R8G8B8A8_UNORM}};
	renderer.ui_program             = device.create_program("gui", gui_state);
	device.compile_graphics_state(renderer.ui_program, {.rasterization = {.culling = false}, .alpha_blending = true});

	renderer.glyph_atlas = device.create_image({
		.name   = "Glyph atlas",
		.size   = int3(atlas_resolution, 1),
		.format = VK_FORMAT_R8_UNORM,
	});

	return renderer;
}

GraphicPass &register_graph(RenderGraph &graph, UiRenderer &renderer, Painter *painter, Handle<TextureDesc> output)
{
	auto glyph_atlas = renderer.glyph_atlas;

	// Upload glyphs
	graph.raw_pass([painter, glyph_atlas](RenderGraph & /*graph*/, PassApi &api, vulkan::ComputeWork &cmd) {
		Vec<VkBufferImageCopy> glyphs_to_upload;
		painter->glyph_cache.process_events([&](const GlyphEvent &event, const GlyphImage *image, int2 pos) {
			if (event.type == GlyphEvent::Type::New && image) {
				auto [p_image, image_offset] = api.upload_buffer.allocate(image->data_size);
				if (p_image.empty()) {
					return false;
				}

				std::memcpy(p_image.data(), image->data, image->data_size);
				auto copy = VkBufferImageCopy{
					.bufferOffset = image_offset,
					.imageSubresource =
						{
							.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
							.layerCount = 1,
						},
					.imageOffset =
						{
							.x = pos.x,
							.y = pos.y,
							.z = 0,
						},
					.imageExtent =
						{
							.width  = image->image_size.x,
							.height = image->image_size.y,
							.depth  = 1,
						},
				};
				glyphs_to_upload.push(copy);
			}
			return true;
		});
		if (!glyphs_to_upload.is_empty()) {
			cmd.barrier(glyph_atlas, vulkan::ImageUsage::TransferDst);
			cmd.copy_buffer_to_image(api.upload_buffer.buffer, glyph_atlas, glyphs_to_upload);
			cmd.barrier(glyph_atlas, vulkan::ImageUsage::GraphicsShaderRead);
		}
	});

	// Draw the UI
	auto ui_program = renderer.ui_program;
	return graph.graphic_pass(output,
		Handle<TextureDesc>::invalid(),
		[painter, output, ui_program](RenderGraph &graph, PassApi &api, vulkan::GraphicsWork &cmd) {
			auto [p_vertices, vert_offset] = api.dynamic_vertex_buffer.allocate(painter->vertex_bytes_offset,
				sizeof(TexturedRect) * sizeof(ColorRect));
			ASSERT(!p_vertices.empty());
			std::memcpy(p_vertices.data(), painter->vertex_buffer.data(), painter->vertex_bytes_offset);

			ASSERT(vert_offset % sizeof(TexturedRect) == 0);
			ASSERT(vert_offset % sizeof(ColorRect) == 0);
			ASSERT(vert_offset % sizeof(Rect) == 0);

			auto [p_indices, ind_offset] =
				api.dynamic_index_buffer.allocate(painter->index_offset * sizeof(PrimitiveIndex),
					sizeof(PrimitiveIndex));
			std::memcpy(p_indices.data(), painter->index_buffer.data(), painter->index_offset * sizeof(PrimitiveIndex));

			PACKED(struct PainterOptions {
				float2 scale;
				float2 translation;
				u32    vertices_descriptor_index;
				u32    primitive_byte_offset;
			})

			auto output_size       = graph.image_size(output);
			auto options           = bindings::bind_option_struct<PainterOptions>(api.device, api.uniform_buffer, cmd);
			options[0].scale       = float2(2.0) / float2(int2(output_size.x, output_size.y));
			options[0].translation = float2(-1.0f, -1.0f);
			options[0].vertices_descriptor_index =
				api.device.get_buffer_storage_index(api.dynamic_vertex_buffer.buffer);
			options[0].primitive_byte_offset = static_cast<u32>(vert_offset);

			cmd.bind_pipeline(ui_program, 0);
			cmd.bind_index_buffer(api.dynamic_index_buffer.buffer, VK_INDEX_TYPE_UINT32, ind_offset);
			cmd.draw_indexed({.vertex_count = painter->index_offset});
		});
}
