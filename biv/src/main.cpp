#include <cross/file_dialog.h>
#include <cross/mapped_file.h>
#include <cross/platform.h>
#include <cross/window.h>
#include <exo/buttons.h>

#include <exo/collections/vector.h>
#include <exo/logger.h>
#include <exo/macros/defer.h>
#include <exo/macros/packed.h>
#include <exo/memory/linear_allocator.h>
#include <exo/memory/scope_stack.h>

#include <render/bindings.h>
#include <render/shader_watcher.h>
#include <render/simple_renderer.h>
#include <render/vulkan/commands.h>
#include <render/vulkan/image.h>
#include <render/vulkan/pipelines.h>

#include "painter/font.h"
#include "painter/glyph_cache.h"
#include "painter/painter.h"
#include "ui/ui.h"

#include "inputs.h"

#include <Tracy.hpp>
#include <filesystem>
#include <spng.h>

inline constexpr int2 GLYPH_ATLAS_RESOLUTION = int2(1024, 1024);

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

// --- Structs

PACKED(struct PushConstants {
	u32 draw_id        = u32_invalid;
	u32 gui_texture_id = u32_invalid;
})

enum struct PixelFormat
{

	R8G8B8A8_UNORM,
	R8G8B8A8_SRGB,
	BC7_SRGB,
	BC7_UNORM,
	BC4_UNORM,
	BC5_UNORM,
};

enum struct ImageExtension
{
	PNG
};

struct Image
{
	PixelFormat    format      = PixelFormat::R8G8B8A8_SRGB;
	ImageExtension extension   = ImageExtension::PNG;
	i32            width       = 0;
	i32            height      = 0;
	i32            depth       = 0;
	i32            levels      = 0;
	Vec<usize>     mip_offsets = {};

	void       *impl_data   = nullptr; // ktxTexture* for libktx, u8* containing raw pixels for png
	const void *pixels_data = nullptr;
	usize       data_size   = 0;
};

struct RenderSample
{
	cross::Platform *platform = nullptr;
	cross::Window   *window   = nullptr;
	Inputs           inputs   = {};

	SimpleRenderer                  renderer;
	Handle<vulkan::GraphicsProgram> ui_program;
	Handle<vulkan::GraphicsProgram> viewer_program;
	Handle<vulkan::Image>           viewer_gpu_image_upload;
	Handle<vulkan::Image>           viewer_gpu_image_current;
	Handle<vulkan::Image>           glyph_atlas;

	Painter *painter = nullptr;

	UiTheme ui_theme;
	UiState ui_state;
	Font    ui_font;
	Rect    viewer_clip_rect = {};

	Image image;
	bool  display_channels[4] = {true, true, true, false};
	u32   viewer_flags        = 0b00000000'00000000'00000000'00001110;
};

const u32 RED_CHANNEL_MASK   = 0b00000000'00000000'00000000'00001000;
const u32 GREEN_CHANNEL_MASK = 0b00000000'00000000'00000000'00000100;
const u32 BLUE_CHANNEL_MASK  = 0b00000000'00000000'00000000'00000010;
const u32 ALPHA_CHANNEL_MASK = 0b00000000'00000000'00000000'00000001;

// --- fwd
static void open_file(RenderSample *app, const std::filesystem::path &path);

// --- App
RenderSample *render_sample_init(exo::ScopeStack &scope)
{
	ZoneScoped;

	auto *app = scope.allocate<RenderSample>();

	app->platform = reinterpret_cast<cross::Platform *>(scope.allocate(cross::platform_get_size()));
	cross::platform_create(app->platform);

	app->window = cross::Window::create(app->platform, scope, {1280, 720}, "Best Image Viewer");
	app->inputs.bind(Action::QuitApp, {.keys = {exo::VirtualKey::Escape}});

	app->renderer = SimpleRenderer::create(app->window->get_win32_hwnd());

	auto *window   = app->window;
	auto &renderer = app->renderer;

	vulkan::GraphicsState gui_state = {};
	gui_state.vertex_shader         = renderer.device.create_shader(SHADER_PATH("ui.vert.glsl.spv"));
	gui_state.fragment_shader       = renderer.device.create_shader(SHADER_PATH("ui.frag.glsl.spv"));
	gui_state.attachments_format    = {.attachments_format = {VK_FORMAT_R8G8B8A8_UNORM}};
	app->ui_program                 = renderer.device.create_program("gui", gui_state);
	renderer.device.compile_graphics_state(app->ui_program,
		{.rasterization = {.culling = false}, .alpha_blending = true});

	vulkan::GraphicsState viewer_state = {};
	viewer_state.vertex_shader         = renderer.device.create_shader(SHADER_PATH("viewer.vert.glsl.spv"));
	viewer_state.fragment_shader       = renderer.device.create_shader(SHADER_PATH("viewer.frag.glsl.spv"));
	viewer_state.attachments_format    = {.attachments_format = {VK_FORMAT_R8G8B8A8_UNORM}};
	app->viewer_program                = renderer.device.create_program("viewer", viewer_state);
	renderer.device.compile_graphics_state(app->viewer_program,
		{.rasterization = {.culling = false}, .alpha_blending = true});

	app->glyph_atlas = app->renderer.device.create_image({
		.name   = "Glyph atlas",
		.size   = int3(GLYPH_ATLAS_RESOLUTION, 1),
		.format = VK_FORMAT_R8_UNORM,
	});

	exo::logger::info("DPI at creation: {}x{}\n", window->get_dpi_scale().x, window->get_dpi_scale().y);

	app->ui_font = Font::from_file(R"(C:\Windows\Fonts\segoeui.ttf)", 13);

	app->painter                      = painter_allocate(scope, 8_MiB, 8_MiB, GLYPH_ATLAS_RESOLUTION);
	app->painter->glyph_atlas_gpu_idx = renderer.device.get_image_sampled_index(app->glyph_atlas);

	app->ui_state.painter   = app->painter;
	app->ui_theme.main_font = &app->ui_font;

	return app;
}

void render_sample_destroy(RenderSample *app)
{
	ZoneScoped;

	cross::platform_destroy(app->platform);
}

struct UiCharCheckbox
{
	char  label;
	Rect  rect;
	bool *value;
};

bool ui_char_checkbox(UiState &ui_state, const UiTheme &ui_theme, const UiCharCheckbox &checkbox)
{
	bool result = checkbox.value ? *checkbox.value : false;
	u64  id     = ui_make_id(ui_state);

	if (ui_is_hovering(ui_state, checkbox.rect)) {
		ui_state.focused = id;
		if (ui_state.active == 0 && ui_state.inputs.mouse_buttons_pressed[exo::MouseButton::Left]) {
			ui_state.active = id;
		}
	}

	if (!ui_state.inputs.mouse_buttons_pressed[exo::MouseButton::Left] && ui_state.focused == id &&
		ui_state.active == id) {
		result = !result;
	}

	u32 border_color = 0xFF8A8A8A;
	if (ui_state.focused == id) {
		if (ui_state.active == id) {
			border_color = 0xFF3D3D3D;
		} else {
			border_color = 0xFFD5D5D5;
		}
	}
	u32 bg_color = 0xFFF3F3F3;
	if (result) {
		bg_color = 0xFFFBA82D;
	}

	float border_thickness = 1.0f;

	const char label_str[] = {checkbox.label, '\0'};
	auto       label_rect =
		rect_center(checkbox.rect, float2(measure_label(*ui_state.painter, *ui_theme.main_font, label_str)));

	ui_push_clip_rect(ui_state, ui_register_clip_rect(ui_state, checkbox.rect));
	painter_draw_color_rect(*ui_state.painter, checkbox.rect, ui_state.i_clip_rect, border_color);
	painter_draw_color_rect(*ui_state.painter,
		rect_inset(checkbox.rect, border_thickness),
		ui_state.i_clip_rect,
		bg_color);
	painter_draw_label(*ui_state.painter, label_rect, ui_state.i_clip_rect, *ui_theme.main_font, label_str);
	ui_pop_clip_rect(ui_state);

	if (checkbox.value && *checkbox.value != result) {
		*checkbox.value = result;
	}
	return result;
}

static void display_ui(RenderSample *app)
{
	app->painter->index_offset        = 0;
	app->painter->vertex_bytes_offset = 0;
	ui_new_frame(app->ui_state);

	auto content_rect = Rect{.pos = {0, 0}, .size = float2(int2(app->window->size.x, app->window->size.y))};

	const float menubar_height_margin = 8.0f;
	const float menu_item_margin      = 12.0f;
	const float menubar_height        = float(app->ui_theme.main_font->metrics.height) + 2.0f * menubar_height_margin;
	Rect        menubar_rect          = rect_split_top(content_rect, menubar_height);

	/* Menu bar */
	const u32 menubar_bg_color = 0xFFF3F3F3;
	painter_draw_color_rect(*app->painter, menubar_rect, app->ui_state.i_clip_rect, menubar_bg_color);

	// add first margin on the left
	rect_split_left(menubar_rect, menu_item_margin);
	auto menubar_theme                    = app->ui_theme;
	menubar_theme.button_bg_color         = 0x00000000;
	menubar_theme.button_hover_bg_color   = 0x06000000;
	menubar_theme.button_pressed_bg_color = 0x09000000;

	auto label_size =
		float2(measure_label(*app->ui_state.painter, *app->ui_theme.main_font, "Open Image")) + float2{8.0f, 0.0f};

	Rect file_rect = rect_split_left(menubar_rect, label_size.x);
	rect_split_left(menubar_rect, menu_item_margin);
	file_rect = rect_center(file_rect, label_size);
	if (ui_button(app->ui_state, menubar_theme, {.label = "Open Image", .rect = file_rect})) {
		if (auto path = cross::file_dialog({{"PNG Image", "*.png"}})) {
			open_file(app, path.value());
		}
	}

	label_size = float2(measure_label(*app->ui_state.painter, *app->ui_theme.main_font, "Help")) + float2{8.0f, 0.0f};
	Rect help_rect = rect_split_left(menubar_rect, label_size.x);
	rect_split_left(menubar_rect, menu_item_margin);

	help_rect = rect_center(help_rect, label_size);
	if (ui_button(app->ui_state, menubar_theme, {.label = "Help", .rect = help_rect})) {
	}

	const auto check_margin = 4.0f;
	const auto check_size   = float2{20.0f};
	Rect       check_rect   = rect_split_left(menubar_rect, check_size.x);
	rect_split_left(menubar_rect, check_margin);

	check_rect = rect_center(check_rect, check_size);
	ui_char_checkbox(app->ui_state,
		menubar_theme,
		{.label = 'R', .rect = check_rect, .value = &app->display_channels[0]});
	app->viewer_flags =
		app->display_channels[0] ? (app->viewer_flags | RED_CHANNEL_MASK) : (app->viewer_flags & ~RED_CHANNEL_MASK);

	check_rect = rect_split_left(menubar_rect, check_size.x);
	rect_split_left(menubar_rect, check_margin);
	check_rect = rect_center(check_rect, check_size);
	ui_char_checkbox(app->ui_state,
		menubar_theme,
		{.label = 'G', .rect = check_rect, .value = &app->display_channels[1]});
	app->viewer_flags =
		app->display_channels[1] ? (app->viewer_flags | GREEN_CHANNEL_MASK) : (app->viewer_flags & ~GREEN_CHANNEL_MASK);

	check_rect = rect_split_left(menubar_rect, check_size.x);
	rect_split_left(menubar_rect, check_margin);
	check_rect = rect_center(check_rect, check_size);
	ui_char_checkbox(app->ui_state,
		menubar_theme,
		{.label = 'B', .rect = check_rect, .value = &app->display_channels[2]});
	app->viewer_flags =
		app->display_channels[2] ? (app->viewer_flags | BLUE_CHANNEL_MASK) : (app->viewer_flags & ~BLUE_CHANNEL_MASK);

	check_rect = rect_split_left(menubar_rect, check_size.x);
	rect_split_left(menubar_rect, menu_item_margin);

	check_rect = rect_center(check_rect, check_size);
	ui_char_checkbox(app->ui_state,
		menubar_theme,
		{.label = 'A', .rect = check_rect, .value = &app->display_channels[3]});
	app->viewer_flags =
		app->display_channels[3] ? (app->viewer_flags | ALPHA_CHANNEL_MASK) : (app->viewer_flags & ~ALPHA_CHANNEL_MASK);

	/* Content */
	auto separator_rect = rect_split_top(content_rect, 1.0f);
	painter_draw_color_rect(*app->painter, separator_rect, app->ui_state.i_clip_rect, 0xFFE5E5E5);

	u32 i_content_rect = ui_register_clip_rect(app->ui_state, content_rect);
	ui_push_clip_rect(app->ui_state, i_content_rect);

	// image viewer
	app->viewer_clip_rect = content_rect;

	ui_pop_clip_rect(app->ui_state);
	ui_end_frame(app->ui_state);
	app->window->set_cursor(static_cast<cross::Cursor>(app->ui_state.cursor));
}

static void render(RenderSample *app)
{
	ZoneScoped;

	auto &renderer = app->renderer;
	auto &graph    = renderer.render_graph;

	auto intermediate_buffer = renderer.render_graph.output(TextureDesc{
		.name = "render buffer desc",
		.size = TextureSize::screen_relative(float2(1.0, 1.0)),
	});
	auto glyph_atlas         = app->glyph_atlas;

	auto *painter = app->painter;
	graph.raw_pass([painter, glyph_atlas](RenderGraph & /*graph*/, PassApi &api, vulkan::ComputeWork &cmd) {
		Vec<VkBufferImageCopy> glyphs_to_upload;
		painter->glyph_cache.process_events([&](const GlyphEvent &event, const GlyphImage *image, int2 pos) {
			if (event.type == GlyphEvent::Type::New && image) {
				auto [p_image, image_offset] = api.upload_buffer.allocate(image->data_size);
				std::memcpy(p_image, image->data, image->data_size);
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
				glyphs_to_upload.push_back(copy);
			}
		});
		painter->glyph_cache.clear_events();
		if (!glyphs_to_upload.empty()) {
			cmd.barrier(glyph_atlas, vulkan::ImageUsage::TransferDst);
			cmd.copy_buffer_to_image(api.upload_buffer.buffer, glyph_atlas, glyphs_to_upload);
			cmd.barrier(glyph_atlas, vulkan::ImageUsage::GraphicsShaderRead);
		}
	});

	auto output     = intermediate_buffer;
	auto ui_program = app->ui_program;
	graph.graphic_pass(output,
		[painter, output, ui_program](RenderGraph &graph, PassApi &api, vulkan::GraphicsWork &cmd) {
			ZoneScopedN("Painter");
			auto [p_vertices, vert_offset] = api.dynamic_vertex_buffer.allocate(painter->vertex_bytes_offset,
				sizeof(TexturedRect) * sizeof(ColorRect));
			std::memcpy(p_vertices, painter->vertices, painter->vertex_bytes_offset);

			ASSERT(vert_offset % sizeof(TexturedRect) == 0);
			ASSERT(vert_offset % sizeof(ColorRect) == 0);
			ASSERT(vert_offset % sizeof(Rect) == 0);

			auto [p_indices, ind_offset] =
				api.dynamic_index_buffer.allocate(painter->index_offset * sizeof(PrimitiveIndex),
					sizeof(PrimitiveIndex));
			std::memcpy(p_indices, painter->indices, painter->index_offset * sizeof(PrimitiveIndex));

			PACKED(struct PainterOptions {
				float2 scale;
				float2 translation;
				u32    vertices_descriptor_index;
				u32    primitive_byte_offset;
			})

			auto  output_size = graph.image_size(output);
			auto *options     = reinterpret_cast<PainterOptions *>(
                bindings::bind_shader_options(api.device, api.uniform_buffer, cmd, sizeof(PainterOptions)));
			options->scale                     = float2(2.0) / float2(int2(output_size.x, output_size.y));
			options->translation               = float2(-1.0f, -1.0f);
			options->vertices_descriptor_index = api.device.get_buffer_storage_index(api.dynamic_vertex_buffer.buffer);
			options->primitive_byte_offset     = static_cast<u32>(vert_offset);

			cmd.bind_pipeline(ui_program, 0);
			cmd.bind_index_buffer(api.dynamic_index_buffer.buffer, VK_INDEX_TYPE_UINT32, ind_offset);
			cmd.draw_indexed({.vertex_count = painter->index_offset});
		});

#if 0
	if (app->viewer_gpu_image_current.is_valid()) {
		graph.graphic_pass([glyph_atlas](RenderGraph &graph, PassApi &api, vulkan::GraphicsWork &cmd) {
			cmd.barrier(app->viewer_gpu_image_current, vulkan::ImageUsage::GraphicsShaderRead);

			PACKED(struct ViewerOptions {
				float2 scale;
				float2 translation;
				u32    texture_descriptor;
				u32    viewer_flags;
				float2 viewport_size;
			})

			float w      = float(app->image.width);
			float h      = float(app->image.height);
			float aspect = w / h;

			float fit_scale = exo::min(app->viewer_clip_rect.size * float2(1.0f / w, 1.0f / h));

			w = fit_scale * w;
			h = fit_scale * h;

			auto *options               = renderer.bind_graphics_shader_options<ViewerOptions>(cmd);
			options->scale.x            = 2.0f * float(w) / app->viewer_clip_rect.size.x;
			options->scale.y            = 2.0f * float(h) / app->viewer_clip_rect.size.y;
			options->translation        = float2(-1.0f, -1.0f);
			options->texture_descriptor = device.get_image_sampled_index(app->viewer_gpu_image_current);
			options->viewer_flags       = app->viewer_flags;
			options->viewport_size      = app->viewer_clip_rect.size;
			cmd.barrier(surface.images[surface.current_image], vulkan::ImageUsage::ColorAttachment);
			cmd.begin_pass(swapchain_framebuffer, std::array{vulkan::LoadOp::load()});
			cmd.set_viewport({
				.x        = (float)app->viewer_clip_rect.pos.x,
				.y        = (float)app->viewer_clip_rect.pos.y,
				.width    = (float)app->viewer_clip_rect.size.x,
				.height   = (float)app->viewer_clip_rect.size.y,
				.minDepth = 0.0f,
				.maxDepth = 1.0f,
			});
			cmd.set_scissor({
				.offset = {.x = (i32)app->viewer_clip_rect.pos.x, .y = (i32)app->viewer_clip_rect.pos.y},
				.extent =
					{
						.width  = (u32)app->viewer_clip_rect.size.x,
						.height = (u32)app->viewer_clip_rect.size.y,
					},
			});
			cmd.bind_pipeline(app->viewer_program, 0);
			cmd.draw({.vertex_count = 6});
			cmd.end_pass();
		});
	}
#endif

	renderer.render(intermediate_buffer, 1.0);
}

static VkFormat to_vk(PixelFormat pformat)
{
	switch (pformat) {
	case PixelFormat::R8G8B8A8_UNORM:
		return VK_FORMAT_R8G8B8A8_UNORM;
	case PixelFormat::R8G8B8A8_SRGB:
		return VK_FORMAT_R8G8B8A8_SRGB;
	case PixelFormat::BC7_SRGB:
		return VK_FORMAT_BC7_SRGB_BLOCK;
	case PixelFormat::BC7_UNORM:
		return VK_FORMAT_BC7_UNORM_BLOCK;
	case PixelFormat::BC4_UNORM:
		return VK_FORMAT_BC4_UNORM_BLOCK;
	case PixelFormat::BC5_UNORM:
		return VK_FORMAT_BC5_UNORM_BLOCK;
	default:
		ASSERT(false);
		return {};
	}
}

static void open_file(RenderSample *app, const std::filesystem::path &path)
{
	ZoneScoped;
	// TODO: PNG importer
	exo::logger::info("Opened file: {}\n", path);

	auto mapped_file = cross::MappedFile::open(path.string());
	if (!mapped_file) {
		return;
	}

	const u8 png_signature[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

	bool is_signature_valid = mapped_file->size > sizeof(png_signature) &&
	                          std::memcmp(mapped_file->base_addr, png_signature, sizeof(png_signature)) == 0;
	if (!is_signature_valid) {
		return;
	}

	spng_ctx *ctx = spng_ctx_new(0);
	DEFER { spng_ctx_free(ctx); };

	spng_set_png_buffer(ctx, mapped_file->base_addr, mapped_file->size);

	struct spng_ihdr ihdr;
	if (spng_get_ihdr(ctx, &ihdr)) {
		return;
	}

	usize decoded_size = 0;
	spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &decoded_size);

	Image &new_image    = app->image;
	new_image.impl_data = reinterpret_cast<u8 *>(malloc(decoded_size));
	spng_decode_image(ctx, new_image.impl_data, decoded_size, SPNG_FMT_RGBA8, 0);

	new_image.extension = ImageExtension::PNG;
	new_image.width     = static_cast<int>(ihdr.width);
	new_image.height    = static_cast<int>(ihdr.height);
	new_image.depth     = 1;
	new_image.levels    = 1;
	new_image.format    = PixelFormat::R8G8B8A8_UNORM;
	new_image.mip_offsets.push_back(0);

	new_image.pixels_data = new_image.impl_data;
	new_image.data_size   = decoded_size;

	app->viewer_gpu_image_upload = app->renderer.device.create_image({
		.name       = "Viewer image",
		.size       = int3(new_image.width, new_image.height, new_image.depth),
		.mip_levels = static_cast<u32>(new_image.levels),
		.format     = to_vk(new_image.format),
	});
}

u8  global_stack_mem[64 << 20];
int main(int /*argc*/, char ** /*argv*/)
{
	exo::LinearAllocator global_allocator =
		exo::LinearAllocator::with_external_memory(global_stack_mem, sizeof(global_stack_mem));
	exo::ScopeStack global_scope = exo::ScopeStack::with_allocator(&global_allocator);
	auto           *app          = render_sample_init(global_scope);
	auto           *window       = app->window;
	auto           &inputs       = app->inputs;

	while (!window->should_close()) {
		window->poll_events();
		inputs.process(window->events);

		if (inputs.is_pressed(Action::QuitApp)) {
			window->stop = true;
		}

		display_ui(app);
		render(app);

		window->events.clear();

		FrameMark
	}
	render_sample_destroy(app);
	return 0;
}
