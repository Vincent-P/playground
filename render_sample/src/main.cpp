#include <exo/os/buttons.h>
#include <exo/os/file_dialog.h>
#include <exo/prelude.h>

#include <exo/collections/dynamic_array.h>
#include <exo/collections/vector.h>
#include <exo/logger.h>
#include <exo/macros/packed.h>
#include <exo/memory/linear_allocator.h>
#include <exo/memory/scope_stack.h>
#include <exo/os/window.h>

#include <engine/render/base_renderer.h>
#include <engine/render/vulkan/context.h>
#include <engine/render/vulkan/device.h>
#include <engine/render/vulkan/surface.h>
namespace gfx = vulkan;

#include <engine/inputs.h>

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

struct RenderSample
{
	exo::Window *window;
	UiTheme ui_theme;
	UiState ui_state;
	Inputs inputs;
	BaseRenderer *renderer;
	Painter *painter;

	FT_Library library;
	hb_buffer_t *current_file_hb_buf = nullptr;
	Vec<u8> current_file_data = {};
	int cursor_offset = 0;

	Font *main_font;
	Font *ui_font;

	Handle<gfx::GraphicsProgram> font_program;

	exo::DynamicArray<Handle<gfx::Framebuffer>, gfx::MAX_SWAPCHAIN_IMAGES> swapchain_framebuffers;
};

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
	surface.destroy_swapchain(device);
	surface.create_swapchain(device);

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
	app->window = exo::Window::create(scope, {1280, 720}, "Render sample");
	app->inputs.bind(Action::QuitApp, {.keys = {exo::VirtualKey::Escape}});

	app->renderer = BaseRenderer::create(scope, app->window,
					     {
						 .push_constant_layout = {.size = sizeof(PushConstants)},
						 .buffer_device_address = false,
					     });

	auto *window = app->window;
	auto *renderer = app->renderer;

	gfx::DescriptorType one_dynamic_buffer_descriptor = {{.count = 1, .type = gfx::DescriptorType::DynamicBuffer}};
	gfx::GraphicsState gui_state = {};
	gui_state.vertex_shader = renderer->device.create_shader("C:/Users/vince/Documents/code/test-vulkan/build/msvc/shaders/"
								 "font.vert.glsl.spv");
	gui_state.fragment_shader = renderer->device.create_shader("C:/Users/vince/Documents/code/test-vulkan/build/msvc/shaders/"
								   "font.frag.glsl.spv");
	gui_state.attachments_format = {.attachments_format = {renderer->surface.format.format}};
	gui_state.descriptors.push_back(one_dynamic_buffer_descriptor);
	app->font_program = renderer->device.create_program("font", gui_state);

	gfx::RenderState state = {.rasterization = {.culling = false}, .alpha_blending = true};
	renderer->device.compile(app->font_program, state);

	app->swapchain_framebuffers.resize(renderer->surface.images.size());
	resize(renderer->device, renderer->surface, app->swapchain_framebuffers);

	auto error = FT_Init_FreeType(&app->library);
	ASSERT(!error);

	exo::logger::info("DPI at creation: {}x{}\n", window->get_dpi_scale().x, window->get_dpi_scale().y);

	app->main_font = font_create(scope, app->renderer, app->library, "C:\\Windows\\Fonts\\JetBrainsMono-Regular.ttf", 13, 4096, VK_FORMAT_R8_UNORM);
	app->current_file_hb_buf = hb_buffer_create();

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

	hb_buffer_destroy(app->current_file_hb_buf);
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

static void display_file(RenderSample *app, const Rect &view_rect)
{
	auto &ui_state = app->ui_state;

	auto &painter = *app->painter;

	const u8 *text = app->current_file_data.data();
	auto *buf = app->current_file_hb_buf;
	auto *font = app->main_font;

	i32 line_height = font->ft_face->size->metrics.height >> 6;

	u32 glyph_count;
	hb_glyph_info_t *glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_count);
	hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);

	i32 cursor_x = static_cast<i32>(view_rect.position.x);
	i32 cursor_y = static_cast<i32>(view_rect.position.y) + line_height;
	for (u32 i = 0; i < glyph_count; i++) {
		u32 glyph_index = glyph_info[i].codepoint;
		auto &cache_entry = font->glyph_cache->get_or_create(glyph_index);
		if (cache_entry.uploaded == false) {
			upload_glyph(app->renderer, font, glyph_index);
		}

		// line wrap each glyph
		if (cursor_x + cache_entry.glyph_size.x > view_rect.position.x + view_rect.size.x) {
			cursor_x = static_cast<i32>(view_rect.position.x);
			cursor_y += line_height;
		}

		if (cursor_y - line_height > view_rect.position.y + view_rect.size.y) {
			break;
		}

		Rect rect = {
		    .position = exo::float2(exo::int2{cursor_x + cache_entry.glyph_top_left.x, cursor_y - cache_entry.glyph_top_left.y}),
		    .size = exo::float2(cache_entry.glyph_size),
		};
		Rect uv = {
		    .position = (1.0f / float(font->cache_resolution)) * exo::float2(cache_entry.x * font->glyph_width_px, cache_entry.y * font->glyph_height_px),
		    .size = (1.0f / float(font->cache_resolution)) * exo::float2(cache_entry.glyph_size),
		};
		painter_draw_textured_rect(painter, rect, ui_state.i_clip_rect, uv, font->glyph_atlas_gpu_idx);

		cursor_x += glyph_pos[i].x_advance >> 6;
		cursor_y += glyph_pos[i].y_advance >> 6;

		if (text[glyph_info[i].cluster] == '\n') {
			cursor_x = static_cast<i32>(view_rect.position.x);
			cursor_y += line_height;
		}
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

	auto label_size = exo::float2(measure_label(app->ui_theme.main_font, "File")) + exo::float2{8.0f, 0.0f};
	auto [file_rect, new_menubar_rect] = rect_split_off_left(menubar_rect, label_size.x, menu_item_margin);
	menubar_rect = new_menubar_rect;
	file_rect = rect_center(file_rect, label_size);
	if (ui_button(app->ui_state, menubar_theme, {.label = "File", .rect = file_rect})) {
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
	static bool r_checked = false;
	if (ui_char_checkbox(app->ui_state, menubar_theme, {.label = 'R', .rect = check_rect, .value = &r_checked})) {
	}

	check_rect = rect_split_off_left(menubar_rect, check_size.x, check_margin).left;
	menubar_rect = rect_split_off_left(menubar_rect, check_size.x, check_margin).right;
	check_rect = rect_center(check_rect, check_size);
	static bool g_checked = false;
	if (ui_char_checkbox(app->ui_state, menubar_theme, {.label = 'G', .rect = check_rect, .value = &g_checked})) {
	}

	check_rect = rect_split_off_left(menubar_rect, check_size.x, check_margin).left;
	menubar_rect = rect_split_off_left(menubar_rect, check_size.x, check_margin).right;
	check_rect = rect_center(check_rect, check_size);
	static bool b_checked = false;
	if (ui_char_checkbox(app->ui_state, menubar_theme, {.label = 'B', .rect = check_rect, .value = &b_checked})) {
	}


	check_rect = rect_split_off_left(menubar_rect, check_size.x, menu_item_margin).left;
	menubar_rect = rect_split_off_left(menubar_rect, check_size.x, menu_item_margin).right;
	check_rect = rect_center(check_rect, check_size);
	static bool a_checked = false;
	if (ui_char_checkbox(app->ui_state, menubar_theme, {.label = 'A', .rect = check_rect, .value = &a_checked})) {
	}

	/* Content */
	auto [separator_rect, content_rect] = rect_split_off_top(rest_rect, 1.0f, 0.0f);
	ui_rect(app->ui_state, app->ui_theme, {.color = 0xFFE5E5E5, .rect = separator_rect});

	u32 i_content_rect = ui_register_clip_rect(app->ui_state, content_rect);
	ui_push_clip_rect(app->ui_state, i_content_rect);

	static float vsplit = 0.5f;
	Rect left_pane = {};
	Rect right_pane = {};
	ui_splitter_x(app->ui_state, app->ui_theme, content_rect, vsplit, left_pane, right_pane);

	/* Left pane */
	static float left_hsplit = 0.5f;
	Rect left_top_pane = {};
	Rect left_bottom_pane = {};
	ui_splitter_y(app->ui_state, app->ui_theme, left_pane, left_hsplit, left_top_pane, left_bottom_pane);

	/* Left top pane */
	u32 i_left_top_pane = ui_register_clip_rect(app->ui_state, left_top_pane);
	ui_push_clip_rect(app->ui_state, i_left_top_pane);
	display_file(app, left_top_pane);
	ui_pop_clip_rect(app->ui_state);

	/* Left bottom pane */

	/* Right pane */
	static float right_hsplit = 0.5f;
	Rect right_top_pane = {};
	Rect right_bottom_pane = {};
	ui_splitter_y(app->ui_state, app->ui_theme, right_pane, right_hsplit, right_top_pane, right_bottom_pane);

	/* Right top pane */
	if (ui_button(app->ui_state, app->ui_theme, {.label = "Button 1", .rect = {.position = right_top_pane.position + exo::float2{10.0f, 10.0f}, .size = {80, 30}}})) {
		exo::logger::info("Button 1 clicked!!\n");
	}

	/* Right Bottom pane */
	Rect open_file_rect = {.position = right_bottom_pane.position + exo::float2{10.0f, 10.0f}, .size = {80, 20}};
	if (ui_button(app->ui_state, app->ui_theme, {.label = "Open file", .rect = open_file_rect})) {
		if (auto fs_path = exo::file_dialog({{"file", "*"}})) {
			open_file(app, fs_path.value());
		}
	}

	char tmp_label[128] = {};
	Rect label_rect = open_file_rect;

	std::memset(tmp_label, 0, sizeof(tmp_label));
	fmt::format_to(tmp_label, "Current file size: {}", app->current_file_data.size());
	label_rect.position.y += label_rect.size.y + 5;
	label_rect.size = exo::float2(measure_label(app->ui_theme.main_font, tmp_label));
	ui_label(app->ui_state, app->ui_theme, {.text = tmp_label, .rect = label_rect});

	std::memset(tmp_label, 0, sizeof(tmp_label));
	fmt::format_to(tmp_label, "UI focused: {}", app->ui_state.focused);
	label_rect.position.y += label_rect.size.y + 5;
	label_rect.size = exo::float2(measure_label(app->ui_theme.main_font, tmp_label));
	ui_label(app->ui_state, app->ui_theme, {.text = tmp_label, .rect = label_rect});

	std::memset(tmp_label, 0, sizeof(tmp_label));
	fmt::format_to(tmp_label, "UI active: {}", app->ui_state.active);
	label_rect.position.y += label_rect.size.y + 5;
	label_rect.size = exo::float2(measure_label(app->ui_theme.main_font, tmp_label));
	ui_label(app->ui_state, app->ui_theme, {.text = tmp_label, .rect = label_rect});

	std::memset(tmp_label, 0, sizeof(tmp_label));
	fmt::format_to(tmp_label, "Frame: {}", app->renderer->frame_count);
	label_rect.position.y += label_rect.size.y + 5;
	label_rect.size = exo::float2(measure_label(app->ui_theme.main_font, tmp_label));
	ui_label(app->ui_state, app->ui_theme, {.text = tmp_label, .rect = label_rect});

	std::memset(tmp_label, 0, sizeof(tmp_label));
	fmt::format_to(tmp_label, "Splitter Y #1: {}", left_hsplit);
	label_rect.position.y += label_rect.size.y + 5;
	label_rect.size = exo::float2(measure_label(app->ui_theme.main_font, tmp_label));
	ui_label(app->ui_state, app->ui_theme, {.text = tmp_label, .rect = label_rect});

	std::memset(tmp_label, 0, sizeof(tmp_label));
	fmt::format_to(tmp_label, "Splitter Y #2: {}", right_hsplit);
	label_rect.position.y += label_rect.size.y + 5;
	label_rect.size = exo::float2(measure_label(app->ui_theme.main_font, tmp_label));
	ui_label(app->ui_state, app->ui_theme, {.text = tmp_label, .rect = label_rect});

	ui_pop_clip_rect(app->ui_state);
	ui_end_frame(app->ui_state);
	app->window->set_cursor(static_cast<exo::Cursor>(app->ui_state.cursor));
}

static void render(RenderSample *app)
{
	ZoneScoped;

	auto &renderer = *app->renderer;
	auto &framebuffers = app->swapchain_framebuffers;
	auto font_program = app->font_program;
	auto *window = app->window;

	auto &device = renderer.device;
	auto &surface = renderer.surface;
	auto &work_pool = renderer.work_pools[renderer.frame_count % FRAME_QUEUE_LENGTH];

	device.wait_idle();
	device.reset_work_pool(work_pool);

	bool out_of_date_swapchain = renderer.start_frame();
	if (out_of_date_swapchain) {
		resize(device, surface, framebuffers);
		return;
	}

	gfx::GraphicsWork cmd = device.get_graphics_work(work_pool);
	cmd.begin();

	auto *global_data = renderer.bind_global_options<u32>();
	global_data[0] = 0;

	device.update_globals();
	cmd.bind_global_set();
	cmd.wait_for_acquired(surface, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

	if (renderer.frame_count == 0) {
		cmd.clear_barrier(app->main_font->glyph_atlas, gfx::ImageUsage::TransferDst);
		cmd.clear_image(app->main_font->glyph_atlas, VkClearColorValue{.float32 = {0.0f, 0.0f, 0.0f, 0.0f}});
		cmd.barrier(app->main_font->glyph_atlas, gfx::ImageUsage::GraphicsShaderRead);
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
			cmd.barrier(device.get_global_sampled_image(image_sampled_idx), gfx::ImageUsage::GraphicsShaderRead);
		}

		PACKED(struct PainterOptions {
			float2 scale;
			float2 translation;
			u32 vertices_descriptor_index;
			u32 primitive_byte_offset;
		})

		auto *options = renderer.bind_shader_options<PainterOptions>(cmd, font_program);
		options->scale = float2(2.0f / window->size.x, 2.0f / window->size.y);
		options->translation = float2(-1.0f, -1.0f);
		options->vertices_descriptor_index = device.get_buffer_storage_index(renderer.dynamic_vertex_buffer.buffer);
		options->primitive_byte_offset = static_cast<u32>(vert_offset);

		cmd.barrier(surface.images[surface.current_image], gfx::ImageUsage::ColorAttachment);
		cmd.begin_pass(swapchain_framebuffer, std::array{gfx::LoadOp::load()});
		cmd.set_viewport({.width = (float)window->size.x, .height = (float)window->size.y, .minDepth = 0.0f, .maxDepth = 1.0f});
		cmd.set_scissor({.extent = {.width = (u32)window->size.x, .height = (u32)window->size.y}});
		cmd.bind_pipeline(font_program, 0);
		cmd.bind_index_buffer(renderer.dynamic_index_buffer.buffer, VK_INDEX_TYPE_UINT32, ind_offset);
		u32 constants[] = {0, 0};
		cmd.push_constant(constants, sizeof(constants));
		cmd.draw_indexed({.vertex_count = painter->index_offset});
		cmd.end_pass();
	}

	cmd.barrier(surface.images[surface.current_image], gfx::ImageUsage::Present);

	cmd.end();

	out_of_date_swapchain = renderer.end_frame(cmd);
	if (out_of_date_swapchain) {
		resize(device, surface, framebuffers);
	}
}

static void open_file(RenderSample *app, const std::filesystem::path &path)
{
	ZoneScoped;

	exo::logger::info("Opened file: {}\n", path);

	std::ifstream file{path, std::ios::binary};
	ASSERT(!file.fail());
	std::streampos begin;
	std::streampos end;
	begin = file.tellg();
	file.seekg(0, std::ios::end);
	end = file.tellg();
	app->current_file_data = Vec<u8>(static_cast<usize>(end - begin));
	file.seekg(0, std::ios::beg);
	file.read(reinterpret_cast<char *>(app->current_file_data.data()), end - begin);
	file.close();

	auto *buf = app->current_file_hb_buf;
	hb_buffer_clear_contents(buf);
	hb_buffer_add_utf8(buf, reinterpret_cast<const char *>(app->current_file_data.data()), static_cast<int>(app->current_file_data.size()), 0, -1);
	hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
	hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
	hb_buffer_set_language(buf, hb_language_from_string("en", -1));

	hb_shape(app->main_font->hb_font, buf, nullptr, 0);
}

u8 global_stack_mem[64 << 20];
int main(int /*argc*/, char ** /*argv*/)
{
	exo::LinearAllocator global_allocator = exo::LinearAllocator::with_external_memory(global_stack_mem, sizeof(global_stack_mem));
	exo::ScopeStack global_scope = exo::ScopeStack::with_allocator(&global_allocator);
	auto *app = render_sample_init(global_scope);
	auto *window = app->window;
	auto &inputs = app->inputs;
	open_file(app, "C:\\Users\\vince\\.emacs.d\\projects");

	window->user_data = app;
	window->paint_callback = +[](void *user_data) {
		auto *app = reinterpret_cast<RenderSample *>(user_data);
		display_ui(app);
		render(app);
	};

	while (!window->should_close()) {
		window->poll_events();

		for (const auto &event : window->events) {
			switch (event.type) {
			case exo::Event::KeyType: {
				break;
			}
			case exo::Event::CharacterType: {
				break;
			}
			default:
				break;
			}
		}
		inputs.process(window->events);

		if (auto scroll = inputs.get_scroll_this_frame()) {
			app->cursor_offset -= scroll->y * 7;
		}
		if (inputs.is_pressed(Action::QuitApp)) {
			window->stop = true;
		}

		window->events.clear();

		FrameMark
	}
	render_sample_destroy(app);
	return 0;
}
