#include <exo/prelude.h>

#include <exo/os/buttons.h>
#include <exo/os/file_dialog.h>
#include <exo/os/mapped_file.h>
#include <exo/os/platform.h>
#include <exo/os/window.h>

#include <exo/collections/dynamic_array.h>
#include <exo/collections/vector.h>
#include <exo/logger.h>
#include <exo/macros/packed.h>
#include <exo/macros/defer.h>
#include <exo/memory/linear_allocator.h>
#include <exo/memory/scope_stack.h>

#include <engine/render/base_renderer.h>
#include <engine/render/vulkan/context.h>
#include <engine/render/vulkan/device.h>
#include <engine/render/vulkan/surface.h>
#include <engine/render/vulkan/utils.h>
namespace gfx = vulkan;

#include <engine/inputs.h>
#include <engine/assets/texture.h>

#include "font.h"
#include "glyph_cache.h"
#include "painter.h"
#include "ui.h"

#include <Tracy.hpp>
#include <array>
#include <filesystem>
#include <fmt/printf.h>
#include <fstream>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb-ft.h>
#include <hb.h>
#include <spng.h>

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
	u32 draw_id = u32_invalid;
	u32 gui_texture_id = u32_invalid;
})

struct Image
{
    PixelFormat format;
    ImageExtension extension;
    i32 width;
    i32 height;
    i32 depth;
    i32 levels;
    Vec<usize> mip_offsets;

    void *impl_data; // ktxTexture* for libktx, u8* containing raw pixels for png
    const void* pixels_data;
    usize data_size;
};

struct RenderSample
{
	exo::Platform *platform;
	exo::Window *window;
	Inputs inputs;

	BaseRenderer *renderer;
	Handle<gfx::GraphicsProgram> ui_program;
	Handle<gfx::GraphicsProgram> viewer_program;
	exo::DynamicArray<Handle<gfx::Framebuffer>, gfx::MAX_SWAPCHAIN_IMAGES> swapchain_framebuffers;
    Handle<gfx::Image> viewer_gpu_image_upload;
    Handle<gfx::Image> viewer_gpu_image_current;

	FT_Library library;
	Painter *painter;

	UiTheme ui_theme;
	UiState ui_state;
	bool       wants_to_open_file = false;
	Font *ui_font;
    Rect viewer_clip_rect;

	Image image;
	bool display_channels[4] = {true, true, true, false};
	u32 viewer_flags = 0b00000000'00000000'00000000'00001110;
};

const u32 RED_CHANNEL_MASK   = 0b00000000'00000000'00000000'00001000;
const u32 GREEN_CHANNEL_MASK = 0b00000000'00000000'00000000'00000100;
const u32 BLUE_CHANNEL_MASK  = 0b00000000'00000000'00000000'00000010;
const u32 ALPHA_CHANNEL_MASK = 0b00000000'00000000'00000000'00000001;

// --- fwd
static void open_file(RenderSample *app, const std::filesystem::path &path);

// --- UI

static Font *font_create(exo::ScopeStack &scope, BaseRenderer *renderer, FT_Library &library, const char *font_path, i32 size_in_pt, u32 cache_resolution, VkFormat cache_format)
{
	auto *font = scope.allocate<Font>();

	auto error = FT_New_Face(library, font_path, 0, &font->ft_face);
	ASSERT(!error);

	font->size_pt = size_in_pt;
	FT_Set_Char_Size(font->ft_face, 0, size_in_pt * 64, 0, 96);

	font->hb_font = hb_ft_font_create_referenced(font->ft_face);
	hb_ft_font_set_funcs(font->hb_font);
	font->label_buf = hb_buffer_create();

	font->glyph_width_px = u32(font->ft_face->bbox.xMax - font->ft_face->bbox.xMin) >> 6;
	font->glyph_height_px = u32(font->ft_face->bbox.yMax - font->ft_face->bbox.yMin) >> 6;

	font->cache_resolution = cache_resolution;
	font->glyph_cache = GlyphCache::create(scope, {
							  .hash_count = 64 << 10,
							  .entry_count = (cache_resolution / font->glyph_width_px) * (cache_resolution / font->glyph_height_px),
							  .glyph_per_row = cache_resolution / font->glyph_width_px,
						      });

	font->glyph_atlas = renderer->device.create_image({
	    .name = "Font atlas",
	    .size = {(i32)cache_resolution, (i32)cache_resolution, 1},
	    .format = cache_format,
	});

	font->glyph_atlas_gpu_idx = renderer->device.get_image_sampled_index(font->glyph_atlas);

	return font;
}

// --- App

static void resize(gfx::Device &device, gfx::Surface &surface, exo::DynamicArray<Handle<gfx::Framebuffer>, gfx::MAX_SWAPCHAIN_IMAGES> &framebuffers)
{
	ZoneScoped;

	device.wait_idle();
	surface.recreate_swapchain(device);

	for (usize i_image = 0; i_image < surface.images.size(); i_image += 1) {
		device.destroy_framebuffer(framebuffers[i_image]);
		framebuffers[i_image] = device.create_framebuffer(
		    {
			.width = surface.width,
			.height = surface.height,
		    },
		    std::array{surface.images[i_image]});
	}
}

RenderSample *render_sample_init(exo::ScopeStack &scope)
{
	ZoneScoped;

	auto *app = scope.allocate<RenderSample>();

	app->platform = reinterpret_cast<exo::Platform*>(scope.allocate(exo::platform_get_size()));
	exo::platform_create(app->platform);

	app->window = exo::Window::create(app->platform, scope, {1280, 720}, "Best Image Viewer");
	app->inputs.bind(Action::QuitApp, {.keys = {exo::VirtualKey::Escape}});

	app->renderer = BaseRenderer::create(scope, app->window,
					     {
						 .push_constant_layout = {.size = sizeof(PushConstants)},
						 .buffer_device_address = false,
					     });

	auto *window = app->window;
	auto *renderer = app->renderer;

#define SHADER_PATH(x) "C:/Users/vince/Documents/code/test-vulkan/build/msvc/shaders/" x
	gfx::GraphicsState gui_state = {};
	gui_state.vertex_shader = renderer->device.create_shader(SHADER_PATH("ui.vert.glsl.spv"));
	gui_state.fragment_shader = renderer->device.create_shader(SHADER_PATH("ui.frag.glsl.spv"));
	gui_state.attachments_format = {.attachments_format = {renderer->surface.format.format}};
	app->ui_program = renderer->device.create_program("gui", gui_state);
	renderer->device.compile(app->ui_program, {.rasterization = {.culling = false}, .alpha_blending = true});

	gfx::GraphicsState viewer_state = {};
	viewer_state.vertex_shader = renderer->device.create_shader(SHADER_PATH("viewer.vert.glsl.spv"));
	viewer_state.fragment_shader = renderer->device.create_shader(SHADER_PATH("viewer.frag.glsl.spv"));
	viewer_state.attachments_format = {.attachments_format = {renderer->surface.format.format}};
	app->viewer_program = renderer->device.create_program("viewer", viewer_state);
	renderer->device.compile(app->viewer_program, {.rasterization = {.culling = false}, .alpha_blending = false});

#undef SHADER_PATH

	app->swapchain_framebuffers.resize(renderer->surface.images.size());
	resize(renderer->device, renderer->surface, app->swapchain_framebuffers);

	auto error = FT_Init_FreeType(&app->library);
	ASSERT(!error);

	exo::logger::info("DPI at creation: {}x{}\n", window->get_dpi_scale().x, window->get_dpi_scale().y);

	app->ui_font = font_create(scope, app->renderer, app->library, "C:\\Windows\\Fonts\\segoeui.ttf", 13, 1024, VK_FORMAT_R8_UNORM);

	app->painter = painter_allocate(scope, 8_MiB, 8_MiB);

	app->ui_state.painter = app->painter;
	app->ui_state.inputs = &app->inputs;
	app->ui_theme.main_font = app->ui_font;

	return app;
}

void render_sample_destroy(RenderSample *app)
{
	ZoneScoped;

	exo::platform_destroy(app->platform);
}

static void upload_glyph(BaseRenderer *renderer, Font *font, u32 glyph_index)
{
	auto &cache_entry = font->glyph_cache->get_or_create(glyph_index);
	if (cache_entry.uploaded) {
		return;
	}

	ZoneScopedN("Upload glyph");

	// Render the glyph with FreeType
	int error = 0;
	{
		ZoneScopedN("Load glyph");
		error = FT_Load_Glyph(font->ft_face, glyph_index, 0);
		ASSERT(!error);
	}

	{
		ZoneScopedN("Render glyph");
		error = FT_Render_Glyph(font->ft_face->glyph, FT_RENDER_MODE_NORMAL);
		ASSERT(!error);
	}

	FT_GlyphSlot slot = font->ft_face->glyph;

	// Upload it to GPU
	const ImageRegion regions[] = {{
	    .image_offset = exo::int2(cache_entry.x * font->glyph_width_px, cache_entry.y * font->glyph_height_px),
	    .image_size = exo::int2(slot->bitmap.width, slot->bitmap.rows),
	    .buffer_size = exo::int2(slot->bitmap.pitch, 0),
	}};

	// Don't upload 0x0 glyphs
	int bitmap_area = regions[0].image_size.x * regions[0].image_size.y;
	bool uploaded = bitmap_area <= 0 ? true : renderer->streamer.upload_image_regions(font->glyph_atlas, slot->bitmap.buffer, slot->bitmap.pitch * slot->bitmap.rows, regions);

	if (uploaded) {
		cache_entry.uploaded = true;
		cache_entry.glyph_top_left = {slot->bitmap_left, slot->bitmap_top};
		cache_entry.glyph_size = {static_cast<int>(slot->bitmap.width), static_cast<int>(slot->bitmap.rows)};
	}
}

struct UiCharCheckbox
{
    char label;
    Rect rect;
    bool *value;
};

bool ui_char_checkbox(UiState &ui_state, const UiTheme &ui_theme, const UiCharCheckbox &checkbox)
{
    bool result = checkbox.value ? *checkbox.value : false;
    u64  id     = ui_make_id(ui_state);

    if (ui_is_hovering(ui_state, checkbox.rect)) {
	    ui_state.focused = id;
	    if (ui_state.active == 0 && ui_state.inputs->mouse_buttons_pressed[exo::MouseButton::Left]) {
		    ui_state.active = id;
	    }
    }

    if (!ui_state.inputs->mouse_buttons_pressed[exo::MouseButton::Left] && ui_state.focused == id && ui_state.active == id) {
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
    if (result)
    {
	bg_color = 0xFFFBA82D;
    }

    float border_thickness = 1.0f;

    const char label_str[] = {checkbox.label, '\0'};
    auto label_rect = rect_center(checkbox.rect, exo::float2(measure_label(ui_theme.main_font, label_str)));

    ui_push_clip_rect(ui_state, ui_register_clip_rect(ui_state, checkbox.rect));
    painter_draw_color_rect(*ui_state.painter, checkbox.rect, ui_state.i_clip_rect, border_color);
    painter_draw_color_rect(*ui_state.painter, rect_inset(checkbox.rect, border_thickness), ui_state.i_clip_rect, bg_color);
    painter_draw_label(*ui_state.painter, label_rect, ui_state.i_clip_rect, ui_theme.main_font, label_str);
    ui_pop_clip_rect(ui_state);

    if (checkbox.value && *checkbox.value != result)
    {
	*checkbox.value = result;
    }
    return result;
}

static void display_ui(RenderSample *app)
{
	app->painter->index_offset = 0;
	app->painter->vertex_bytes_offset = 0;
	ui_new_frame(app->ui_state);

	auto fullscreen_rect = Rect{.position = {0, 0}, .size = exo::float2(app->window->size.x, app->window->size.y)};

	const float menubar_height_margin   = 8.0f;
	const float menu_item_margin = 12.0f;
	const float menubar_height          = float(app->ui_theme.main_font->ft_face->size->metrics.height >> 6) + 2.0f * menubar_height_margin;
	auto [menubar_rect, rest_rect] = rect_split_off_top(fullscreen_rect, menubar_height, 0.0f);

	/* Menu bar */
	const u32 menubar_bg_color = 0xFFF3F3F3;
	ui_rect(app->ui_state, app->ui_theme, {.color = menubar_bg_color, .rect = menubar_rect});

	menubar_rect = rect_split_off_left(menubar_rect, 0.0f, menu_item_margin).right; // add first margin on the left
	auto menubar_theme = app->ui_theme;
	menubar_theme.button_bg_color = 0x00000000;
	menubar_theme.button_hover_bg_color = 0x06000000;
	menubar_theme.button_pressed_bg_color = 0x09000000;

	auto label_size = exo::float2(measure_label(app->ui_theme.main_font, "Open Image")) + exo::float2{8.0f, 0.0f};
	auto [file_rect, new_menubar_rect] = rect_split_off_left(menubar_rect, label_size.x, menu_item_margin);
	menubar_rect = new_menubar_rect;
	file_rect = rect_center(file_rect, label_size);
	if (ui_button(app->ui_state, menubar_theme, {.label = "Open Image", .rect = file_rect})) {
		app->wants_to_open_file = true;

		if (app->wants_to_open_file)
		{
		    if (auto path = exo::file_dialog({{"PNG Image", "*.png"}}))
		    {
				open_file(app, path.value());
		    }
		    app->wants_to_open_file = false;
		}
	}

	label_size = exo::float2(measure_label(app->ui_theme.main_font, "Help")) + exo::float2{8.0f, 0.0f};
	auto [help_rect, new_menubar_rect2] = rect_split_off_left(menubar_rect, label_size.x, menu_item_margin);
	menubar_rect = new_menubar_rect2;
	help_rect = rect_center(help_rect, label_size);
	if (ui_button(app->ui_state, menubar_theme, {.label = "Help", .rect = help_rect})) {
	}

	const auto check_margin = 4.0f;
	const auto check_size = exo::float2{20.0f};
	auto [check_rect, new_menubar_rect3] = rect_split_off_left(menubar_rect, check_size.x, check_margin);
	menubar_rect = new_menubar_rect3;
	check_rect = rect_center(check_rect, check_size);
	ui_char_checkbox(app->ui_state, menubar_theme, {.label = 'R', .rect = check_rect, .value = &app->display_channels[0]});
	app->viewer_flags = app->display_channels[0] ? (app->viewer_flags | RED_CHANNEL_MASK) : (app->viewer_flags & ~RED_CHANNEL_MASK);

	check_rect = rect_split_off_left(menubar_rect, check_size.x, check_margin).left;
	menubar_rect = rect_split_off_left(menubar_rect, check_size.x, check_margin).right;
	check_rect = rect_center(check_rect, check_size);
	ui_char_checkbox(app->ui_state, menubar_theme, {.label = 'G', .rect = check_rect, .value = &app->display_channels[1]});
	app->viewer_flags = app->display_channels[1] ? (app->viewer_flags | GREEN_CHANNEL_MASK) : (app->viewer_flags & ~GREEN_CHANNEL_MASK);

	check_rect = rect_split_off_left(menubar_rect, check_size.x, check_margin).left;
	menubar_rect = rect_split_off_left(menubar_rect, check_size.x, check_margin).right;
	check_rect = rect_center(check_rect, check_size);
	ui_char_checkbox(app->ui_state, menubar_theme, {.label = 'B', .rect = check_rect, .value = &app->display_channels[2]});
	app->viewer_flags = app->display_channels[2] ? (app->viewer_flags | BLUE_CHANNEL_MASK) : (app->viewer_flags & ~BLUE_CHANNEL_MASK);


	check_rect = rect_split_off_left(menubar_rect, check_size.x, menu_item_margin).left;
	menubar_rect = rect_split_off_left(menubar_rect, check_size.x, menu_item_margin).right;
	check_rect = rect_center(check_rect, check_size);
	ui_char_checkbox(app->ui_state, menubar_theme, {.label = 'A', .rect = check_rect, .value = &app->display_channels[3]});
	app->viewer_flags = app->display_channels[3] ? (app->viewer_flags | ALPHA_CHANNEL_MASK) : (app->viewer_flags & ~ALPHA_CHANNEL_MASK);

	/* Content */
	auto [separator_rect, content_rect] = rect_split_off_top(rest_rect, 1.0f, 0.0f);
	ui_rect(app->ui_state, app->ui_theme, {.color = 0xFFE5E5E5, .rect = separator_rect});

	u32 i_content_rect = ui_register_clip_rect(app->ui_state, content_rect);
	ui_push_clip_rect(app->ui_state, i_content_rect);

	// image viewer
	app->viewer_clip_rect = content_rect;

	ui_pop_clip_rect(app->ui_state);
	ui_end_frame(app->ui_state);
	app->window->set_cursor(static_cast<exo::Cursor>(app->ui_state.cursor));
}

static void render(RenderSample *app, bool has_resize)
{
	ZoneScoped;

	auto &renderer = *app->renderer;
	auto &framebuffers = app->swapchain_framebuffers;
	auto ui_program = app->ui_program;
	auto *window = app->window;

	auto &device = renderer.device;
	auto &surface = renderer.surface;
	auto &work_pool = renderer.work_pools[renderer.frame_count % FRAME_QUEUE_LENGTH];

	if (has_resize)
	{
		resize(device, surface, framebuffers);
	}

	bool out_of_date_swapchain = renderer.start_frame();
	if (out_of_date_swapchain) {
		resize(device, surface, framebuffers);
		return;
	}

	gfx::GraphicsWork cmd = device.get_graphics_work(work_pool);
	cmd.begin();

	auto *global_data = renderer.bind_global_options<u32>(cmd);
	global_data[0] = 0;

	device.update_globals();
	cmd.wait_for_acquired(surface, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

	if (renderer.frame_count == 0) {
		cmd.clear_barrier(app->ui_font->glyph_atlas, gfx::ImageUsage::TransferDst);
		cmd.clear_image(app->ui_font->glyph_atlas, VkClearColorValue{.float32 = {0.0f, 0.0f, 0.0f, 0.0f}});
		cmd.barrier(app->ui_font->glyph_atlas, gfx::ImageUsage::GraphicsShaderRead);
	}

	if (app->viewer_gpu_image_upload != app->viewer_gpu_image_current)
	{
		renderer.streamer.upload_image_full(app->viewer_gpu_image_upload, app->image.pixels_data, app->image.data_size);
		app->viewer_gpu_image_current = app->viewer_gpu_image_upload;
	}

	renderer.streamer.update(cmd);

	auto swapchain_framebuffer = framebuffers[surface.current_image];

	cmd.clear_barrier(surface.images[surface.current_image], gfx::ImageUsage::ColorAttachment);
	cmd.begin_pass(swapchain_framebuffer, std::array{gfx::LoadOp::clear({.color = {.float32 = {1.0f, 1.0f, 1.0f, 1.0f}}})});
	cmd.end_pass();

	{
		auto *painter = app->painter;
		ZoneScopedN("Painter");
		auto [p_vertices, vert_offset] = renderer.dynamic_vertex_buffer.allocate(renderer.device, painter->vertex_bytes_offset, sizeof(TexturedRect) * sizeof(ColorRect));
		std::memcpy(p_vertices, painter->vertices, painter->vertex_bytes_offset);

		ASSERT(vert_offset % sizeof(TexturedRect) == 0);
		ASSERT(vert_offset % sizeof(ColorRect) == 0);
		ASSERT(vert_offset % sizeof(Rect) == 0);

		auto [p_indices, ind_offset] = renderer.dynamic_index_buffer.allocate(renderer.device, painter->index_offset * sizeof(PrimitiveIndex), sizeof(PrimitiveIndex));
		std::memcpy(p_indices, painter->indices, painter->index_offset * sizeof(PrimitiveIndex));

		for (const auto &font_glyph : painter->glyphs_to_upload) {
			upload_glyph(app->renderer, font_glyph.font, font_glyph.glyph_index);
		}

		for (u32 image_sampled_idx : painter->used_textures) {
			cmd.barrier(gfx::get_sampler_image_at(device.global_sets.bindless, image_sampled_idx), gfx::ImageUsage::GraphicsShaderRead);
		}

		PACKED(struct PainterOptions {
			float2 scale;
			float2 translation;
			u32 vertices_descriptor_index;
			u32 primitive_byte_offset;
		})

		auto *options = renderer.bind_graphics_shader_options<PainterOptions>(cmd);
		options->scale = float2(2.0f / window->size.x, 2.0f / window->size.y);
		options->translation = float2(-1.0f, -1.0f);
		options->vertices_descriptor_index = device.get_buffer_storage_index(renderer.dynamic_vertex_buffer.buffer);
		options->primitive_byte_offset = static_cast<u32>(vert_offset);

		cmd.barrier(surface.images[surface.current_image], gfx::ImageUsage::ColorAttachment);
		cmd.begin_pass(swapchain_framebuffer, std::array{gfx::LoadOp::load()});
		cmd.set_viewport({.width = (float)window->size.x, .height = (float)window->size.y, .minDepth = 0.0f, .maxDepth = 1.0f});
		cmd.set_scissor({.extent = {.width = (u32)window->size.x, .height = (u32)window->size.y}});
		cmd.bind_pipeline(ui_program, 0);
		cmd.bind_index_buffer(renderer.dynamic_index_buffer.buffer, VK_INDEX_TYPE_UINT32, ind_offset);
		u32 constants[] = {0, 0};
		cmd.push_constant(constants, sizeof(constants));
		cmd.draw_indexed({.vertex_count = painter->index_offset});
		cmd.end_pass();
	}

	if (app->viewer_gpu_image_current.is_valid())
	{
		ZoneScopedN("Viewer");
		cmd.barrier(app->viewer_gpu_image_current, gfx::ImageUsage::GraphicsShaderRead);

		PACKED(struct ViewerOptions {
			float2 scale;
			float2 translation;
			u32 texture_descriptor;
			u32 viewer_flags;
		})

		float w = float(app->image.width);
		float h = float(app->image.height);
		float aspect = w / h;

		if (app->image.width < app->viewer_clip_rect.size.x && app->image.height < app->viewer_clip_rect.size.y)
		{
			int2 dist = int2(app->viewer_clip_rect.size) - int2(app->image.width, app->image.height);
			if (dist.x < dist.y)
			{
				w = app->viewer_clip_rect.size.x;
				h = w / aspect ;
			}
			else
			{
				h = app->viewer_clip_rect.size.y;
				w = h * aspect;
			}
		}

		auto *options               = renderer.bind_graphics_shader_options<ViewerOptions>(cmd);
		options->scale.x            = 2.0f * float(w) / app->viewer_clip_rect.size.x;
		options->scale.y            = 2.0f * float(h) / app->viewer_clip_rect.size.y;
		options->translation        = float2(-1.0f, -1.0f);
		options->texture_descriptor = device.get_image_sampled_index(app->viewer_gpu_image_current);
		options->viewer_flags       = app->viewer_flags;
		cmd.barrier(surface.images[surface.current_image], gfx::ImageUsage::ColorAttachment);
		cmd.begin_pass(swapchain_framebuffer, std::array{gfx::LoadOp::load()});
		cmd.set_viewport({
			.x        = (float)app->viewer_clip_rect.position.x,
			.y        = (float)app->viewer_clip_rect.position.y,
			.width    = (float)app->viewer_clip_rect.size.x,
			.height   = (float)app->viewer_clip_rect.size.y,
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		});
		cmd.set_scissor({
				.offset = {.x = (i32)app->viewer_clip_rect.position.x, .y = (i32)app->viewer_clip_rect.position.y},
			.extent =
				{
					.width  = (u32)app->viewer_clip_rect.size.x,
					.height = (u32)app->viewer_clip_rect.size.y,
				},
		});
		cmd.bind_pipeline(app->viewer_program, 0);
		u32 constants[] = {0, 0};
		cmd.push_constant(constants, sizeof(constants));
		cmd.draw({.vertex_count = 6});
		cmd.end_pass();
	}

	cmd.barrier(surface.images[surface.current_image], gfx::ImageUsage::Present);

	cmd.end();

	out_of_date_swapchain = renderer.end_frame(cmd);
	if (out_of_date_swapchain) {
		resize(device, surface, framebuffers);
	}
}

static VkFormat to_vk(PixelFormat pformat)
{
    // clang-format off
    switch (pformat)
    {
    case PixelFormat::R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
    case PixelFormat::R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
    case PixelFormat::BC7_SRGB: return VK_FORMAT_BC7_SRGB_BLOCK;
    case PixelFormat::BC7_UNORM: return VK_FORMAT_BC7_UNORM_BLOCK;
    case PixelFormat::BC4_UNORM: return VK_FORMAT_BC4_UNORM_BLOCK;
    case PixelFormat::BC5_UNORM: return VK_FORMAT_BC5_UNORM_BLOCK;
    default: ASSERT(false);
    }
    // clang-format on
    exo::unreachable();
}

static void open_file(RenderSample *app, const std::filesystem::path &path)
{
	ZoneScoped;
	//TODO: PNG importer
	exo::logger::info("Opened file: {}\n", path);

	auto mapped_file = exo::MappedFile::open(path.string());
	if (!mapped_file) {
		return;
	}

    const u8 png_signature[] = {
        0x89,
        0x50,
        0x4E,
        0x47,
        0x0D,
        0x0A,
        0x1A,
        0x0A
    };

	bool is_signature_valid = mapped_file->size > sizeof(png_signature)
		&& std::memcmp(mapped_file->base_addr, png_signature, sizeof(png_signature)) == 0;
	if (!is_signature_valid) {
		return;
	}

    spng_ctx *ctx = spng_ctx_new(0);
    DEFER
    {
        spng_ctx_free(ctx);
    };

    spng_set_png_buffer(ctx, mapped_file->base_addr, mapped_file->size);

    struct spng_ihdr ihdr;
    if (spng_get_ihdr(ctx, &ihdr))
    {
        return;
    }

    usize decoded_size = 0;
    spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &decoded_size);

    Image &new_image      = app->image;
    new_image.impl_data = reinterpret_cast<u8 *>(malloc(decoded_size));
    spng_decode_image(ctx, new_image.impl_data, decoded_size, SPNG_FMT_RGBA8, 0);

    new_image.extension = ImageExtension::PNG;
    new_image.width     = static_cast<int>(ihdr.width);
    new_image.height    = static_cast<int>(ihdr.height);
    new_image.depth     = 1;
    new_image.levels    = 1;
    new_image.format    = PixelFormat::R8G8B8A8_UNORM;
    new_image.mip_offsets.push_back(0);

    new_image.pixels_data  = new_image.impl_data;
    new_image.data_size = decoded_size;

	app->viewer_gpu_image_upload = app->renderer->device.create_image({
                .name       = "Viewer image",
                .size       = exo::int3(new_image.width, new_image.height, new_image.depth),
                .mip_levels = static_cast<u32>(new_image.levels),
                .format     = to_vk(new_image.format),
		});
}

u8 global_stack_mem[64 << 20];
int main(int /*argc*/, char ** /*argv*/)
{
	exo::LinearAllocator global_allocator = exo::LinearAllocator::with_external_memory(global_stack_mem, sizeof(global_stack_mem));
	exo::ScopeStack global_scope = exo::ScopeStack::with_allocator(&global_allocator);
	auto *app = render_sample_init(global_scope);
	auto *window = app->window;
	auto &inputs = app->inputs;

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

		inputs.process(window->events);

		if (inputs.is_pressed(Action::QuitApp)) {
			window->stop = true;
		}

		display_ui(app);
		render(app, has_resize);

		window->events.clear();

		FrameMark
	}
	render_sample_destroy(app);
	return 0;
}
