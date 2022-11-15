#include <cross/buttons.h>
#include <cross/file_dialog.h>
#include <cross/mapped_file.h>
#include <cross/platform.h>
#include <cross/window.h>

#include <exo/collections/vector.h>
#include <exo/format.h>
#include <exo/logger.h>
#include <exo/macros/defer.h>
#include <exo/macros/packed.h>
#include <exo/memory/linear_allocator.h>
#include <exo/memory/scope_stack.h>
#include <exo/profile.h>
#include <exo/string_view.h>
#include <render/bindings.h>
#include <render/shader_watcher.h>
#include <render/simple_renderer.h>
#include <render/vulkan/commands.h>
#include <render/vulkan/image.h>
#include <render/vulkan/pipelines.h>
#include <ui_renderer/ui_renderer.h>

#include "painter/font.h"
#include "painter/glyph_cache.h"
#include "painter/painter.h"
#include "ui/ui.h"

#include "inputs.h"

#include <filesystem>
#include <spng.h>

inline constexpr int2 GLYPH_ATLAS_RESOLUTION = int2(1024, 1024);

void *operator new(std::size_t count)
{
	auto ptr = malloc(count);
	EXO_PROFILE_MALLOC(ptr, count);
	return ptr;
}

void operator delete(void *ptr) noexcept
{
	EXO_PROFILE_MFREE(ptr);
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
	std::unique_ptr<cross::Window> window = nullptr;
	Inputs                         inputs = {};

	SimpleRenderer                  renderer;
	UiRenderer                      ui_renderer;
	Handle<vulkan::GraphicsProgram> viewer_program;
	Handle<vulkan::Image>           viewer_gpu_image_upload;
	Handle<vulkan::Image>           viewer_gpu_image_current;

	Painter *painter = nullptr;

	ui::Ui ui;
	Font   ui_font;
	Rect   viewer_clip_rect = {};

	Image image;
	bool  display_channels[4] = {true, true, true, false};
	u32   viewer_flags        = 0b00000000'00000000'00000000'00001110;
};

const u32 RED_CHANNEL_MASK   = 0b00000000'00000000'00000000'00001000;
const u32 GREEN_CHANNEL_MASK = 0b00000000'00000000'00000000'00000100;
const u32 BLUE_CHANNEL_MASK  = 0b00000000'00000000'00000000'00000010;
const u32 ALPHA_CHANNEL_MASK = 0b00000000'00000000'00000000'00000001;

// --- fwd
static void open_file(RenderSample *app, const exo::StringView &path);

// --- App
RenderSample *render_sample_init(exo::ScopeStack &scope)
{
	EXO_PROFILE_SCOPE;

	auto *app = scope.allocate<RenderSample>();

	auto *platform = reinterpret_cast<cross::platform::Platform *>(scope.allocate(cross::platform::get_size()));
	cross::platform::create(platform);

	app->window = cross::Window::create({1280, 720}, "Best Image Viewer");
	app->inputs.bind(Action::QuitApp, {.keys = {exo::VirtualKey::Escape}});

	app->renderer  = SimpleRenderer::create(app->window->get_win32_hwnd());
	auto &renderer = app->renderer;

	app->ui_renderer = UiRenderer::create(renderer.device, GLYPH_ATLAS_RESOLUTION);

	vulkan::GraphicsState viewer_state = {};
	viewer_state.vertex_shader         = renderer.device.create_shader(SHADER_PATH("viewer.vert.glsl.spv"));
	viewer_state.fragment_shader       = renderer.device.create_shader(SHADER_PATH("viewer.frag.glsl.spv"));
	viewer_state.attachments_format    = {.attachments_format = {VK_FORMAT_R8G8B8A8_UNORM}};
	app->viewer_program                = renderer.device.create_program("viewer", viewer_state);
	renderer.device.compile_graphics_state(app->viewer_program,
		{.rasterization = {.culling = false}, .alpha_blending = true});

	exo::logger::info("DPI at creation: %dx%d\n", app->window->get_dpi_scale().x, app->window->get_dpi_scale().y);

	app->ui_font                      = Font::from_file(R"(C:\Windows\Fonts\segoeui.ttf)", 13);
	app->painter                      = painter_allocate(scope, 8_MiB, 8_MiB, GLYPH_ATLAS_RESOLUTION);
	app->painter->glyph_atlas_gpu_idx = renderer.device.get_image_sampled_index(app->ui_renderer.glyph_atlas);

	app->ui = ui::create(&app->ui_font, 14.0f, app->painter);

	return app;
}

void render_sample_destroy(RenderSample * /*app*/)
{
	EXO_PROFILE_SCOPE;

	cross::platform::destroy();
}

namespace ui
{
struct CharCheckbox
{
	char  label;
	Rect  rect;
	bool *value;
};

bool char_checkbox(Ui &ui, const CharCheckbox &checkbox)
{
	bool      result = checkbox.value ? *checkbox.value : false;
	const u64 id     = make_id(ui);

	if (is_hovering(ui, checkbox.rect)) {
		ui.activation.focused = id;
		if (ui.activation.active == 0 && ui.inputs.mouse_buttons_pressed[exo::MouseButton::Left]) {
			ui.activation.active = id;
		}
	}

	if (!ui.inputs.mouse_buttons_pressed[exo::MouseButton::Left] && ui.activation.focused == id &&
		ui.activation.active == id) {
		result = !result;
	}

	auto border_color = ColorU32::from_greyscale(u8(0x8A));
	if (ui.activation.focused == id) {
		if (ui.activation.active == id) {
			border_color = ColorU32::from_greyscale(u8(0x3D));
		} else {
			border_color = ColorU32::from_greyscale(u8(0xD5));
		}
	}
	auto bg_color = ColorU32::from_greyscale(u8(0xF3));
	if (result) {
		bg_color = ColorU32::from_uints(0x2D, 0xA8, 0xFB);
	}

	const float border_thickness = 1.0f;

	const char label_str[] = {checkbox.label, '\0'};
	auto label_rect = rect_center(checkbox.rect, float2(measure_label(*ui.painter, *ui.theme.main_font, label_str)));

	push_clip_rect(ui, register_clip_rect(ui, checkbox.rect));
	painter_draw_color_rect(*ui.painter, checkbox.rect, ui.state.i_clip_rect, border_color);
	painter_draw_color_rect(*ui.painter, rect_inset(checkbox.rect, border_thickness), ui.state.i_clip_rect, bg_color);
	painter_draw_label(*ui.painter, label_rect, ui.state.i_clip_rect, *ui.theme.main_font, label_str);
	pop_clip_rect(ui);

	if (checkbox.value && *checkbox.value != result) {
		*checkbox.value = result;
	}
	return result;
}
} // namespace ui

static void display_ui(RenderSample *app)
{
	app->painter->index_offset        = 0;
	app->painter->vertex_bytes_offset = 0;
	ui::new_frame(app->ui);

	auto content_rect = Rect{.pos = {0, 0}, .size = float2(int2(app->window->size.x, app->window->size.y))};

	const float menubar_height_margin = 8.0f;
	const float menu_item_margin      = 12.0f;
	const float menubar_height        = float(app->ui.theme.main_font->metrics.height) + 2.0f * menubar_height_margin;
	Rect        menubar_rect          = rect_split_top(content_rect, menubar_height);

	/* Menu bar */
	const auto menubar_bg_color = ColorU32::from_greyscale(u8(0xF3));
	painter_draw_color_rect(*app->ui.painter, menubar_rect, app->ui.state.i_clip_rect, menubar_bg_color);

	// add first margin on the left
	rect_split_left(menubar_rect, menu_item_margin);
	auto menubar_theme                    = app->ui.theme;
	menubar_theme.button_bg_color         = ColorU32::from_uints(0, 0, 0, 0x00);
	menubar_theme.button_hover_bg_color   = ColorU32::from_uints(0, 0, 0, 0x06);
	menubar_theme.button_pressed_bg_color = ColorU32::from_uints(0, 0, 0, 0x09);

	auto label_size =
		float2(measure_label(*app->ui.painter, *app->ui.theme.main_font, "Open Image")) + float2{8.0f, 0.0f};

	Rect file_rect = rect_split_left(menubar_rect, label_size.x);
	rect_split_left(menubar_rect, menu_item_margin);
	file_rect = rect_center(file_rect, label_size);
	if (ui::button(app->ui, {.label = "Open Image", .rect = file_rect})) {
		auto png_extension = std::make_pair(exo::String{"PNG Image"}, exo::String{"*.png"});
		if (auto path = cross::file_dialog({&png_extension, 1})) {
			open_file(app, path.value());
		}
	}

	label_size     = float2(measure_label(*app->ui.painter, *app->ui.theme.main_font, "Help")) + float2{8.0f, 0.0f};
	Rect help_rect = rect_split_left(menubar_rect, label_size.x);
	rect_split_left(menubar_rect, menu_item_margin);

	help_rect = rect_center(help_rect, label_size);
	if (ui::button(app->ui, {.label = "Help", .rect = help_rect})) {
	}

	const auto check_margin = 4.0f;
	const auto check_size   = float2{20.0f};
	Rect       check_rect   = rect_split_left(menubar_rect, check_size.x);
	rect_split_left(menubar_rect, check_margin);

	check_rect = rect_center(check_rect, check_size);
	ui::char_checkbox(app->ui, {.label = 'R', .rect = check_rect, .value = &app->display_channels[0]});
	app->viewer_flags =
		app->display_channels[0] ? (app->viewer_flags | RED_CHANNEL_MASK) : (app->viewer_flags & ~RED_CHANNEL_MASK);

	check_rect = rect_split_left(menubar_rect, check_size.x);
	rect_split_left(menubar_rect, check_margin);
	check_rect = rect_center(check_rect, check_size);
	ui::char_checkbox(app->ui, {.label = 'G', .rect = check_rect, .value = &app->display_channels[1]});
	app->viewer_flags =
		app->display_channels[1] ? (app->viewer_flags | GREEN_CHANNEL_MASK) : (app->viewer_flags & ~GREEN_CHANNEL_MASK);

	check_rect = rect_split_left(menubar_rect, check_size.x);
	rect_split_left(menubar_rect, check_margin);
	check_rect = rect_center(check_rect, check_size);
	ui::char_checkbox(app->ui, {.label = 'B', .rect = check_rect, .value = &app->display_channels[2]});
	app->viewer_flags =
		app->display_channels[2] ? (app->viewer_flags | BLUE_CHANNEL_MASK) : (app->viewer_flags & ~BLUE_CHANNEL_MASK);

	check_rect = rect_split_left(menubar_rect, check_size.x);
	rect_split_left(menubar_rect, menu_item_margin);

	check_rect = rect_center(check_rect, check_size);
	ui::char_checkbox(app->ui, {.label = 'A', .rect = check_rect, .value = &app->display_channels[3]});
	app->viewer_flags =
		app->display_channels[3] ? (app->viewer_flags | ALPHA_CHANNEL_MASK) : (app->viewer_flags & ~ALPHA_CHANNEL_MASK);

	/* Content */
	auto separator_rect = rect_split_top(content_rect, 1.0f);
	painter_draw_color_rect(*app->ui.painter,
		separator_rect,
		app->ui.state.i_clip_rect,
		ColorU32::from_greyscale(u8(0xE5)));

	const u32 i_content_rect = ui::register_clip_rect(app->ui, content_rect);
	ui::push_clip_rect(app->ui, i_content_rect);

	// image viewer
	app->viewer_clip_rect = content_rect;

	ui::pop_clip_rect(app->ui);
	ui::end_frame(app->ui);
	app->window->set_cursor(static_cast<cross::Cursor>(app->ui.state.cursor));
}

static void render(RenderSample *app)
{
	EXO_PROFILE_SCOPE;

	auto &renderer = app->renderer;
	auto &graph    = renderer.render_graph;

	auto intermediate_buffer = renderer.render_graph.output(TextureDesc{
		.name = "render buffer desc",
		.size = TextureSize::screen_relative(float2(1.0, 1.0)),
	});

	register_graph(graph, app->ui_renderer, app->painter, intermediate_buffer);

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

static void open_file(RenderSample *app, const exo::StringView &path)
{
	EXO_PROFILE_SCOPE;
	// TODO: PNG importer
	exo::logger::info("Opened file: %.*s\n", path.size(), path.data());

	auto mapped_file = cross::MappedFile::open(path);
	if (!mapped_file) {
		return;
	}

	const u8 png_signature[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

	const bool is_signature_valid = mapped_file->size > sizeof(png_signature) &&
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
	new_image.mip_offsets.push(0);

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
	auto           &window       = app->window;
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

		EXO_PROFILE_FRAMEMARK;
	}
	render_sample_destroy(app);
	return 0;
}
