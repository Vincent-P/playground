#include <exo/os/buttons.h>
#include <exo/os/file_dialog.h>
#include <exo/prelude.h>

#include <exo/logger.h>
#include <exo/macros/packed.h>
#include <exo/memory/linear_allocator.h>
#include <exo/memory/scope_stack.h>
#include <exo/collections/dynamic_array.h>
#include <exo/collections/vector.h>
#include <exo/os/window.h>

#include <engine/render/vulkan/context.h>
#include <engine/render/vulkan/device.h>
#include <engine/render/vulkan/surface.h>
#include <engine/render/base_renderer.h>
#include <engine/render/imgui_pass.h>
namespace gfx = vulkan;

#include <engine/inputs.h>
#include <engine/ui.h>

#include "glyph_cache.h"

#include <Tracy.hpp>
#include <array>
#include <imgui.h>
#include <fstream>
#include <filesystem>
#include <fmt/printf.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>

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

PACKED(struct Rect
{
    exo::float2 position;
    exo::float2 size;
})

PACKED(struct ColorRect
{
    Rect rect;
    u32 color;
    u32 padding[3];
})

PACKED(struct TexturedRect
{
    Rect rect;
    Rect uv;
    u32 texture_descriptor;
    u32 padding[3];
})

enum RectType
{
    RectType_Color,
    RectType_Textured,
};

union PrimitiveIndex
{
    struct
    {
        u32 index : 24;
        u32 corner : 2;
        u32 type : 6;
    };
    u32 raw;
};
static_assert(sizeof(PrimitiveIndex) == sizeof(u32));

struct Font
{
    i32          size_pt;
    u32 glyph_width_px;
    u32 glyph_height_px;
    u32          cache_resolution;

    FT_Face      ft_face;
    hb_font_t   *hb_font   = nullptr;
    hb_buffer_t *label_buf = nullptr;

    GlyphCache  *glyph_cache;
    Handle<gfx::Image> glyph_atlas;
};

struct Painter
{
    u8 *vertices;
    PrimitiveIndex *indices;
    BaseRenderer *renderer;

    usize vertices_size;
    usize indices_size;
    usize vertex_bytes_offset;
    u32 index_offset;

    // hold a list of texture referenced by TexturedRect to make barrier before rendering
};

struct UiTheme
{
    u32 button_bg_color = 0xFFDA901E;
    u32 button_hover_bg_color = 0xFFD58100;
    u32 button_pressed_bg_color = 0xFFBC7200;
    u32 button_label_color = 0xFFFFFFFF;

    Font *main_font;
};

struct UiState
{
    u64 focused = 0;
    u64 active = 0;
    u64 gen = 0;

    Inputs *inputs = nullptr;
    Painter *painter = nullptr;
};

struct UiButton
{
    const char *label;
    Rect rect;
};

PACKED(struct PushConstants {
    u32 draw_id        = u32_invalid;
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

    FT_Library   library;
    hb_buffer_t *current_file_hb_buf = nullptr;
    Vec<u8>      current_file_data   = {};
    int          cursor_offset       = 0;

    Font *main_font;
    Font *ui_font;

    ImGuiPass                    imgui_pass = {};
    Handle<gfx::GraphicsProgram> font_program;

    exo::DynamicArray<Handle<gfx::Framebuffer>, gfx::MAX_SWAPCHAIN_IMAGES> swapchain_framebuffers;
};

// --- fwd
static void open_file(RenderSample *app, const std::filesystem::path &path);
static void upload_glyph(BaseRenderer *renderer, Font *font, u32 glyph_index, GlyphEntry &cache_entry);

// --- UI

static Painter *painter_allocate(exo::ScopeStack &scope, BaseRenderer *renderer, usize vertex_buffer_size, usize index_buffer_size)
{
    auto *painter = scope.allocate<Painter>();
    painter->renderer = renderer;
    painter->vertices = reinterpret_cast<u8*>(scope.allocate(vertex_buffer_size));
    painter->indices = reinterpret_cast<PrimitiveIndex*>(scope.allocate(index_buffer_size));

    painter->vertices_size = vertex_buffer_size;
    painter->indices_size = index_buffer_size;

    std::memset(painter->vertices, 0, vertex_buffer_size);
    std::memset(painter->indices, 0, index_buffer_size);

    painter->vertex_bytes_offset = 0;
    painter->index_offset = 0;
    return painter;
}

static void painter_draw_textured_rect(Painter &painter, const Rect &rect, const Rect &uv, u32 texture)
{
    auto misalignment = painter.vertex_bytes_offset % sizeof(TexturedRect);
    if (misalignment != 0)
    {
        painter.vertex_bytes_offset += sizeof(TexturedRect) - misalignment;
    }

    ASSERT(painter.vertex_bytes_offset % sizeof(TexturedRect) == 0);
    u32 i_rect = painter.vertex_bytes_offset / sizeof(TexturedRect);
    auto *vertices = reinterpret_cast<TexturedRect*>(painter.vertices);
    vertices[i_rect] = {.rect = rect, .uv = uv, .texture_descriptor = texture};
    painter.vertex_bytes_offset += sizeof(TexturedRect);

    // 0 - 3
    // |   |
    // 1 - 2
    painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 0, .type = RectType_Textured}};
    painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 1, .type = RectType_Textured}};
    painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 2, .type = RectType_Textured}};
    painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 2, .type = RectType_Textured}};
    painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 3, .type = RectType_Textured}};
    painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 0, .type = RectType_Textured}};

    ASSERT(painter.index_offset * sizeof(PrimitiveIndex) < painter.indices_size);
    ASSERT(painter.vertex_bytes_offset < painter.vertices_size);
}

static void painter_draw_color_rect(Painter &painter, const Rect &rect, u32 AABBGGRR)
{
    auto misalignment = painter.vertex_bytes_offset % sizeof(ColorRect);
    if (misalignment != 0)
    {
        painter.vertex_bytes_offset += sizeof(ColorRect) - misalignment;
    }

    ASSERT(painter.vertex_bytes_offset % sizeof(ColorRect) == 0);
    u32 i_rect = painter.vertex_bytes_offset / sizeof(ColorRect);
    auto *vertices = reinterpret_cast<ColorRect*>(painter.vertices);
    vertices[i_rect] = {.rect = rect, .color = AABBGGRR};
    painter.vertex_bytes_offset += sizeof(ColorRect);

    // 0 - 3
    // |   |
    // 1 - 2
    painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 0, .type = RectType_Color}};
    painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 1, .type = RectType_Color}};
    painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 2, .type = RectType_Color}};
    painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 2, .type = RectType_Color}};
    painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 3, .type = RectType_Color}};
    painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 0, .type = RectType_Color}};

    ASSERT(painter.index_offset * sizeof(PrimitiveIndex) < painter.indices_size);
    ASSERT(painter.vertex_bytes_offset < painter.vertices_size);
}

static exo::int2 measure_label(Font *font, const char *label)
{
    auto *buf = font->label_buf;
    hb_buffer_clear_contents(buf);
    hb_buffer_add_utf8(buf, label, -1, 0, -1);
    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buf, hb_language_from_string("en", -1));

    hb_shape(font->hb_font, buf, nullptr, 0);

    u32                  glyph_count;
    i32                  line_height = font->ft_face->size->metrics.height >> 6;
    hb_glyph_info_t     *glyph_info  = hb_buffer_get_glyph_infos(buf, &glyph_count);
    hb_glyph_position_t *glyph_pos   = hb_buffer_get_glyph_positions(buf, &glyph_count);

    i32 width = 0;
    for (u32 i = 0; i < glyph_count; i++)
    {
        auto &cache_entry = font->glyph_cache->get_or_create(glyph_info[i].codepoint);
        width += (glyph_pos[i].x_advance >> 6) + cache_entry.glyph_size.x;
    }

    return {width, line_height};
}

static void painter_draw_label(Painter &painter, const Rect &rect, Font *font, const char *label)
{
    auto *buf = font->label_buf;
    hb_buffer_clear_contents(buf);
    hb_buffer_add_utf8(buf, label, -1, 0, -1);
    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buf, hb_language_from_string("en", -1));

    hb_shape(font->hb_font, buf, nullptr, 0);

    u32                  glyph_count;
    i32                  line_height = font->ft_face->size->metrics.height >> 6;
    hb_glyph_info_t     *glyph_info  = hb_buffer_get_glyph_infos(buf, &glyph_count);
    hb_glyph_position_t *glyph_pos   = hb_buffer_get_glyph_positions(buf, &glyph_count);

    i32 cursor_x = rect.position.x;
    i32 cursor_y = rect.position.y + line_height;
    for (u32 i = 0; i < glyph_count; i++)
    {
        u32 glyph_index   = glyph_info[i].codepoint;
        i32  x_advance = glyph_pos[i].x_advance;
        i32  y_advance = glyph_pos[i].y_advance;

        auto &cache_entry = font->glyph_cache->get_or_create(glyph_index);

        if (cache_entry.uploaded == false)
        {
            upload_glyph(painter.renderer, font, glyph_index, cache_entry);
        }

        Rect rect = {
            .position = exo::float2(exo::int2{cursor_x + cache_entry.glyph_top_left.x, cursor_y - cache_entry.glyph_top_left.y}),
            .size = exo::float2(cache_entry.glyph_size),
        };
        Rect uv = {.position = {(cache_entry.x * font->glyph_width_px) / float(font->cache_resolution),
                                (cache_entry.y * font->glyph_height_px) / float(font->cache_resolution)},
                   .size     = {cache_entry.glyph_size.x / float(font->cache_resolution),
                            cache_entry.glyph_size.y / float(font->cache_resolution)}
        };
        painter_draw_textured_rect(painter, rect, uv, painter.renderer->device.get_image_sampled_index(font->glyph_atlas));

        cursor_x += x_advance >> 6;
        cursor_y += y_advance >> 6;

        if (label[glyph_info[i].cluster] == '\n')
        {
            cursor_x = rect.position.x;
            cursor_y += line_height;
        }
    }
}

static Font *font_create(exo::ScopeStack &scope,
                         BaseRenderer *renderer,
                         FT_Library &library,
                         const char *font_path,
                         i32 size_in_pt,
                         u32 cache_resolution,
                         VkFormat cache_format)
{
    auto *font = scope.allocate<Font>();

    auto error = FT_New_Face(library, font_path, 0, &font->ft_face);
    ASSERT (!error);

    font->size_pt = size_in_pt;
    FT_Set_Char_Size(font->ft_face, 0, size_in_pt*64, 0, 96);

    font->hb_font = hb_ft_font_create_referenced(font->ft_face);
    hb_ft_font_set_funcs(font->hb_font);
    font->label_buf = hb_buffer_create();

    font->glyph_width_px = u32(font->ft_face->bbox.xMax - font->ft_face->bbox.xMin) >> 6;
    font->glyph_height_px = u32(font->ft_face->bbox.yMax - font->ft_face->bbox.yMin) >> 6;

    font->cache_resolution = cache_resolution;
    font->glyph_cache = GlyphCache::create(scope,
                             {
                                 .hash_count  = 64 << 10,
                                 .entry_count = (cache_resolution / font->glyph_width_px) * (cache_resolution / font->glyph_height_px),
                                 .glyph_per_row = cache_resolution / font->glyph_width_px,
                             });


    font->glyph_atlas = renderer->device.create_image({
        .name   = "Font atlas",
        .size   = {(i32)cache_resolution, (i32)cache_resolution, 1},
        .format = cache_format,
    });

    return font;
}
static void ui_new_frame(UiState &ui_state)
{
    ui_state.gen = 0;
    ui_state.focused = 0;
}

static void ui_end_frame(UiState &ui_state)
{
    if (!ui_state.inputs->mouse_buttons_pressed[exo::MouseButton::Left])
    {
        ui_state.active = 0;
    }
    else
    {
        // active = invalid means no widget are focused
        if (ui_state.active == 0)
        {
            ui_state.active = u64_invalid;
        }
    }
}

static bool ui_is_hovering(const UiState &ui_state, const Rect &rect)
{
    return rect.position.x <= ui_state.inputs->mouse_position.x && ui_state.inputs->mouse_position.x <= rect.position.x + rect.size.x
        && rect.position.y <= ui_state.inputs->mouse_position.y && ui_state.inputs->mouse_position.y <= rect.position.y + rect.size.y;
}

static u64 ui_make_id(UiState &state)
{
    return ++state.gen;
}

static bool ui_button(u64 id, UiState &ui_state, const UiTheme &ui_theme, const UiButton &button)
{
    bool result = false;
    if (id == 0)
    {
        id = ui_make_id(ui_state);
    }

    // behavior
    if (ui_is_hovering(ui_state, button.rect))
    {
        ui_state.focused = id;
        if (ui_state.active == 0 && ui_state.inputs->mouse_buttons_pressed[exo::MouseButton::Left])
        {
            ui_state.active = id;
        }
    }

    if (!ui_state.inputs->mouse_buttons_pressed[exo::MouseButton::Left] && ui_state.focused == id && ui_state.active == id)
    {
        result = true;
    }

    // rendering
    if (ui_state.focused == id)
    {
        if (ui_state.active == id)
        {
            painter_draw_color_rect(*ui_state.painter, button.rect, ui_theme.button_pressed_bg_color);
        }
        else
        {
            painter_draw_color_rect(*ui_state.painter, button.rect, ui_theme.button_hover_bg_color);
        }
    }
    else
    {
        painter_draw_color_rect(*ui_state.painter, button.rect, ui_theme.button_bg_color);
    }

    auto label_rect = button.rect;
    label_rect.size = exo::float2(measure_label(ui_theme.main_font, button.label));
    label_rect.position.x += (button.rect.size.x - label_rect.size.x) / 2.0f;
    label_rect.position.y += (button.rect.size.y - label_rect.size.y) / 2.0f;

    painter_draw_color_rect(*ui_state.painter, label_rect, 0xFF0000FF);
    painter_draw_label(*ui_state.painter, label_rect, ui_theme.main_font, button.label);

    return result;
}

static void ui_splitter_x(u64 id, UiState &ui_state, const UiTheme &ui_theme, const Rect &view_rect, float &value, Rect &left, Rect &right)
{
    if (id == 0)
    {
        id = ui_make_id(ui_state);
    }

    Rect splitter_input = Rect{.position = {view_rect.position.x + view_rect.size.x * value - 5.0f, view_rect.position.y}, .size = {10.0f, view_rect.size.y}};

    // behavior
    if (ui_is_hovering(ui_state, splitter_input))
    {
        ui_state.focused = id;
        if (ui_state.active == 0 && ui_state.inputs->mouse_buttons_pressed[exo::MouseButton::Left])
        {
            ui_state.active = id;
        }
    }

    if (ui_state.active == id)
    {
        value = float(ui_state.inputs->mouse_position.x - view_rect.position.x) / float(view_rect.size.x);
    }

    left.position = view_rect.position;
    left.size = {view_rect.size.x * value, view_rect.size.y};

    right.position = {view_rect.position.x + left.size.x, view_rect.position.y};
    right.size = {view_rect.size.x * (1.0f - value), view_rect.size.y};

    // painter_draw_color_rect(*ui_state.painter, splitter_input, 0x440000FF);
    if (ui_state.focused == id)
    {
        painter_draw_color_rect(*ui_state.painter, {.position = {right.position.x - 2, view_rect.position.y}, .size = {4, view_rect.size.y}}, 0xFF888888);
    }
    else
    {
        painter_draw_color_rect(*ui_state.painter, {.position = {right.position.x - 1, view_rect.position.y}, .size = {2, view_rect.size.y}}, 0xFF666666);
    }
}

static void ui_splitter_y(u64 id, UiState &ui_state, const UiTheme &ui_theme, const Rect &view_rect, float &value, Rect &top, Rect &bottom)
{
    if (id == 0)
    {
        id = ui_make_id(ui_state);
    }

    Rect splitter_input = Rect{.position = {view_rect.position.x, view_rect.position.y + view_rect.size.y * value - 5.0f }, .size = {view_rect.size.x, 10.0f}};

    // behavior
    if (ui_is_hovering(ui_state, splitter_input))
    {
        ui_state.focused = id;
        if (ui_state.active == 0 && ui_state.inputs->mouse_buttons_pressed[exo::MouseButton::Left])
        {
            ui_state.active = id;
        }
    }

    if (ui_state.active == id)
    {
        value = float(ui_state.inputs->mouse_position.y - view_rect.position.y) / float(view_rect.size.y);
    }

    top.position = view_rect.position;
    top.size = {view_rect.size.x, view_rect.size.y * value};

    bottom.position = {view_rect.position.x, view_rect.position.y + top.size.y};
    bottom.size = {view_rect.size.x, view_rect.size.y * (1.0f - value)};

    // painter_draw_color_rect(*ui_state.painter, splitter_input, 0x440000FF);
    if (ui_state.focused == id)
    {
        painter_draw_color_rect(*ui_state.painter, {.position = {view_rect.position.x, bottom.position.y - 2}, .size = {view_rect.size.x, 4}}, 0xFF888888);
    }
    else
    {
        painter_draw_color_rect(*ui_state.painter, {.position = {view_rect.position.x, bottom.position.y - 1}, .size = {view_rect.size.x, 2}}, 0xFF666666);
    }

    // painter_draw_color_rect(*ui_state.painter, splitter_input, 0x440000FF);
}

// --- App

static void resize(gfx::Device &device, gfx::Surface &surface, exo::DynamicArray<Handle<gfx::Framebuffer>, gfx::MAX_SWAPCHAIN_IMAGES> &framebuffers)
{
    ZoneScoped;

    device.wait_idle();
    surface.destroy_swapchain(device);
    surface.create_swapchain(device);

    for (usize i_image = 0; i_image < surface.images.size(); i_image += 1)
    {
        device.destroy_framebuffer(framebuffers[i_image]);
        framebuffers[i_image] = device.create_framebuffer(
            {
                .width  = surface.width,
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

    app->renderer = BaseRenderer::create(scope,
                                                   app->window,
                                                   {
                                                       .push_constant_layout  = {.size = sizeof(PushConstants)},
                                                       .buffer_device_address = false,
                                                   });

    auto *window = app->window;
    auto *renderer = app->renderer;

    gfx::DescriptorType one_dynamic_buffer_descriptor = {{{.count = 1, .type = gfx::DescriptorType::DynamicBuffer}}};
    gfx::GraphicsState gui_state = {};
    gui_state.vertex_shader      = renderer->device.create_shader("C:/Users/vince/Documents/code/test-vulkan/build/msvc/shaders/font.vert.glsl.spv");
    gui_state.fragment_shader    = renderer->device.create_shader("C:/Users/vince/Documents/code/test-vulkan/build/msvc/shaders/font.frag.glsl.spv");
    gui_state.attachments_format = {.attachments_format = {renderer->surface.format.format}};
    gui_state.descriptors.push_back(one_dynamic_buffer_descriptor);
    app->font_program = renderer->device.create_program("font", gui_state);

    gfx::RenderState state = {.rasterization = {.culling = false}, .alpha_blending = true};
    renderer->device.compile(app->font_program, state);

    app->swapchain_framebuffers.resize(renderer->surface.images.size());
    resize(renderer->device, renderer->surface, app->swapchain_framebuffers);

    UI::create_context(window, &app->inputs);
    imgui_pass_init(renderer->device, app->imgui_pass, renderer->surface.format.format);

    auto error = FT_Init_FreeType(&app->library);
    ASSERT (!error);

    exo::logger::info("DPI at creation: {}x{}\n", window->get_dpi_scale().x, window->get_dpi_scale().y);

    app->main_font = font_create(scope, app->renderer, app->library, "C:\\Windows\\Fonts\\JetBrainsMono-Regular.ttf", 13, 4096, VK_FORMAT_R8_UNORM);
    app->current_file_hb_buf = hb_buffer_create();

    app->ui_font = font_create(scope, app->renderer, app->library, "C:\\Windows\\Fonts\\segoeui.ttf", 13, 1024, VK_FORMAT_R8_UNORM);

    app->painter = painter_allocate(scope, app->renderer, 8_MiB, 8_MiB);

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

static void upload_glyph(BaseRenderer *renderer, Font *font, u32 glyph_index, GlyphEntry &cache_entry)
{
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
        .image_size   = exo::int2(slot->bitmap.width, slot->bitmap.rows),
        .buffer_size  = exo::int2(slot->bitmap.pitch, 0),
    }};

    bool uploaded = renderer->streamer.upload_image_regions(font->glyph_atlas,
                                                            slot->bitmap.buffer,
                                                            slot->bitmap.pitch * slot->bitmap.rows,
                                                            regions);

    if (uploaded)
    {
        cache_entry.uploaded       = true;
        cache_entry.glyph_top_left = {slot->bitmap_left, slot->bitmap_top};
        cache_entry.glyph_size     = {static_cast<int>(slot->bitmap.width), static_cast<int>(slot->bitmap.rows)};
    }
}

static void display_file(RenderSample *app, const Rect &view_rect)
{
    auto &ui_state = app->ui_state;
    const auto &ui_theme = app->ui_theme;

    auto &painter = *app->painter;

    const u8 *text = app->current_file_data.data();
    auto *buf = app->current_file_hb_buf;
    auto *font = app->main_font;

    i32           line_height = font->ft_face->size->metrics.height >> 6;

    u32         glyph_count;
    hb_glyph_info_t     *glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_count);
    hb_glyph_position_t *glyph_pos  = hb_buffer_get_glyph_positions(buf, &glyph_count);

    i32 cursor_x = static_cast<i32>(view_rect.position.x);
    i32 cursor_y = static_cast<i32>(view_rect.position.y) + line_height;
    for (u32 i = 0; i < glyph_count; i++)
    {
        u32 glyph_index   = glyph_info[i].codepoint;
        auto &cache_entry = font->glyph_cache->get_or_create(glyph_index);
        if (cache_entry.uploaded == false)
        {
            upload_glyph(app->renderer, font, glyph_index, cache_entry);
        }

        // line wrap each glyph
        if (cursor_x + cache_entry.glyph_size.x > view_rect.position.x + view_rect.size.x)
        {
            cursor_x = static_cast<i32>(view_rect.position.x);
            cursor_y += line_height;
        }

        if (cursor_y - line_height > view_rect.position.y + view_rect.size.y)
        {
            break;
        }

        Rect rect = {
            .position = exo::float2(exo::int2{cursor_x + cache_entry.glyph_top_left.x, cursor_y - cache_entry.glyph_top_left.y}),
            .size = exo::float2(cache_entry.glyph_size),
        };
        Rect uv = {.position = {(cache_entry.x * font->glyph_width_px) / float(font->cache_resolution),
                                (cache_entry.y * font->glyph_height_px) / float(font->cache_resolution)},
                   .size     = {cache_entry.glyph_size.x / float(font->cache_resolution),
                            cache_entry.glyph_size.y / float(font->cache_resolution)}
        };
        painter_draw_textured_rect(painter, rect, uv, app->renderer->device.get_image_sampled_index(font->glyph_atlas));

        cursor_x += glyph_pos[i].x_advance >> 6;
        cursor_y += glyph_pos[i].y_advance >> 6;

        if (text[glyph_info[i].cluster] == '\n')
        {
            cursor_x = static_cast<i32>(view_rect.position.x);
            cursor_y += line_height;
        }
    }
}

static void display_ui(RenderSample *app)
{
    app->painter->index_offset = 0;
    app->painter->vertex_bytes_offset = 0;
    ui_new_frame(app->ui_state);

    auto fullscreen_rect = Rect{.position = {0, 0}, .size = exo::float2(app->window->size.x, app->window->size.y)};

    static char input_command_buffer[128] = "Hello, world!";
    if (ImGui::InputText("Command", input_command_buffer, sizeof(input_command_buffer), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        exo::logger::info("Command: {}\n", input_command_buffer);
        std::memset(input_command_buffer, 0, sizeof(input_command_buffer));
    }

    ImGui::Text("Current file size: %zu", app->current_file_data.size());

    // painter test
    #if 0
    painter_draw_color_rect(*app->painter, {.position = {250, 250}, .size = {100, 100}}, 0xaa0000ff);
    painter_draw_color_rect(*app->painter, {.position = {260, 260}, .size = {100, 100}}, 0xaaff0000);
    painter_draw_label(*app->painter, {.position = {250, 250}, .size = {100, 20}}, app->ui_font, "Test label");
    #endif

    Rect left_pane = {};
    Rect right_pane = {};
    static float vsplit = 0.5f;
    ui_splitter_x(0, app->ui_state, app->ui_theme, fullscreen_rect, vsplit, left_pane, right_pane);


    Rect left_top_pane = {};
    Rect left_bottom_pane = {};

    static float left_hsplit = 0.5f;
    ui_splitter_y(0, app->ui_state, app->ui_theme, left_pane, left_hsplit, left_top_pane, left_bottom_pane);

    display_file(app, left_top_pane);

    Rect right_top_pane = {};
    Rect right_bottom_pane = {};

    static float right_hsplit = 0.5f;
    ui_splitter_y(0, app->ui_state, app->ui_theme, right_pane, right_hsplit, right_top_pane, right_bottom_pane);

    // ui test
    if (ui_button(0, app->ui_state, app->ui_theme, {.label = "Button 1", .rect = {.position = right_top_pane.position + exo::float2{10.0f, 10.0f}, .size = {156, 45}}}))
    {
        exo::logger::info("Button 1 clicked!!\n");
    }

    if (ui_button(0, app->ui_state, app->ui_theme, {.label = "Open file", .rect = {.position = right_bottom_pane.position + exo::float2{10.0f, 10.0f}, .size = {156, 45}}}))
    {
        if (auto fs_path = exo::file_dialog({{"file", "*"}}))
        {
            open_file(app, fs_path.value());
        }
    }

    ImGui::Text("UI focused: %zu", app->ui_state.focused);
    ImGui::Text("UI active: %zu", app->ui_state.active);

    ui_end_frame(app->ui_state);
}

static void render(RenderSample *app)
{
    ZoneScoped;

    auto &renderer = *app->renderer;
    auto & framebuffers = app->swapchain_framebuffers;
    auto &imgui_pass = app->imgui_pass;
    auto font_program = app->font_program;
    auto *window = app->window;

    auto &device = renderer.device;
    auto &surface = renderer.surface;
    auto &work_pool       = renderer.work_pools[renderer.frame_count % FRAME_QUEUE_LENGTH];

    device.wait_idle();
    device.reset_work_pool(work_pool);

    bool out_of_date_swapchain = renderer.start_frame();
    if (out_of_date_swapchain)
    {
        resize(device, surface, framebuffers);
        ImGui::EndFrame();
        return;
    }

    gfx::GraphicsWork cmd = device.get_graphics_work(work_pool);
    cmd.begin();

    auto *global_data = renderer.bind_global_options<u32>();
    global_data[0] = 0;

    device.update_globals();
    cmd.bind_global_set();
    cmd.wait_for_acquired(surface, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    if (renderer.frame_count == 0)
    {
        auto & io           = ImGui::GetIO();
        uchar *pixels       = nullptr;
        int    imgui_width  = 0;
        int    imgui_height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &imgui_width, &imgui_height);
        ASSERT(imgui_width > 0 && imgui_height > 0);
        u32 width  = static_cast<u32>(imgui_width);
        u32 height = static_cast<u32>(imgui_height);
        bool uploaded = renderer.streamer.upload_image_full(imgui_pass.font_atlas, pixels, width * height * sizeof(u32));
        ASSERT(uploaded);

        cmd.clear_barrier(app->main_font->glyph_atlas, gfx::ImageUsage::TransferDst);
        cmd.clear_image(app->main_font->glyph_atlas, VkClearColorValue{.float32 = {0.0f, 0.0f, 0.0f, 0.0f}});
        cmd.barrier(app->main_font->glyph_atlas, gfx::ImageUsage::GraphicsShaderRead);
    }
    renderer.streamer.update(cmd);

    auto swapchain_framebuffer = framebuffers[surface.current_image];

    cmd.clear_barrier(surface.images[surface.current_image], gfx::ImageUsage::ColorAttachment);
    cmd.begin_pass(swapchain_framebuffer, std::array{gfx::LoadOp::clear({.color = {.float32 = {1.0f, 1.0f, 1.0f, 1.0f}}})});
    cmd.end_pass();

    ImGui::Text("Frame: %u", renderer.frame_count);
    ImGui::Image((void*)(u64)renderer.device.get_image_sampled_index(app->main_font->glyph_atlas), exo::float2(256, 256));

    ImGui::Render();
    imgui_pass_draw(renderer, imgui_pass, cmd, swapchain_framebuffer);

    {
    auto *painter = app->painter;
    ZoneScopedN("Painter");
    auto [p_vertices, vert_offset] = renderer.dynamic_vertex_buffer.allocate(renderer.device, painter->vertex_bytes_offset, sizeof(TexturedRect) * sizeof(ColorRect));
    std::memcpy(p_vertices, painter->vertices, painter->vertex_bytes_offset);

    auto [p_indices, ind_offset] = renderer.dynamic_index_buffer.allocate(renderer.device, painter->index_offset * sizeof(PrimitiveIndex), sizeof(PrimitiveIndex));
    std::memcpy(p_indices, painter->indices, painter->index_offset * sizeof(PrimitiveIndex));

    PACKED(struct PainterOptions {
        float2 scale;
        float2 translation;
        u32    vertices_descriptor_index;
        u32 primitive_byte_offset;
    })

    auto *options = renderer.bind_shader_options<PainterOptions>(cmd, font_program);
    options->scale = float2(2.0f / window->size.x, 2.0f / window->size.y);
    options->translation = float2(-1.0f, -1.0f);
    options->vertices_descriptor_index = device.get_buffer_storage_index(renderer.dynamic_vertex_buffer.buffer);
    options->primitive_byte_offset = vert_offset;

    cmd.barrier(app->main_font->glyph_atlas, gfx::ImageUsage::GraphicsShaderRead);
    cmd.barrier(surface.images[surface.current_image], gfx::ImageUsage::ColorAttachment);
    cmd.begin_pass(swapchain_framebuffer, std::array{gfx::LoadOp::load()});
    cmd.set_viewport({.width = (float)window->size.x, .height = (float)window->size.y, .minDepth = 0.0f, .maxDepth = 1.0f});
    cmd.set_scissor({.extent = {.width = (u32)window->size.x, .height = (u32)window->size.y}});
    cmd.bind_pipeline(font_program, 0);
    cmd.bind_index_buffer(renderer.dynamic_index_buffer.buffer, VK_INDEX_TYPE_UINT32, ind_offset);
    u32 constants[] = {0, 0};
    cmd.push_constant(constants, sizeof(constants));
    cmd.draw_indexed({.vertex_count  = painter->index_offset});
    cmd.end_pass();
    }


    cmd.barrier(surface.images[surface.current_image], gfx::ImageUsage::Present);

    cmd.end();

    out_of_date_swapchain = renderer.end_frame(cmd);
    if (out_of_date_swapchain)
    {
        resize(device, surface, framebuffers);
    }
}

static void open_file(RenderSample *app, const std::filesystem::path &path)
{
    ZoneScoped;

    exo::logger::info("Opened file: {}\n", path);

    std::ifstream file{path, std::ios::binary};
    ASSERT (!file.fail());
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
    hb_buffer_add_utf8(buf, reinterpret_cast<const char *>(app->current_file_data.data()), app->current_file_data.size(), 0, -1);
    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buf, hb_language_from_string("en", -1));

    hb_shape(app->main_font->hb_font, buf, nullptr, 0);
}

u8  global_stack_mem[64 << 20];
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
        auto *app = reinterpret_cast<RenderSample*>(user_data);
        UI::new_frame();
        display_ui(app);
        render(app);
    };

    while (!window->should_close())
    {
        window->poll_events();

        for (const auto &event : window->events)
        {
            auto &io = ImGui::GetIO();
            switch (event.type)
            {
            case exo::Event::KeyType:
            {
                if (ImGui::IsKeyDown(static_cast<int>(event.key.key)) && event.key.state == exo::ButtonState::Released) {
                    io.KeysDown[static_cast<uint>(event.key.key)] = false;
                }
                else if (!ImGui::IsKeyDown(static_cast<int>(event.key.key)) && event.key.state == exo::ButtonState::Pressed) {
                    io.KeysDown[static_cast<uint>(event.key.key)] = true;
                }
                break;
            }
            case exo::Event::CharacterType:
            {
                io.AddInputCharactersUTF8(event.character.sequence);
                break;
            }
            default:
                break;
            }
        }
        inputs.process(window->events);

        if (auto scroll = inputs.get_scroll_this_frame())
        {
            app->cursor_offset -= scroll->y * 7;
        }
        if (inputs.is_pressed(Action::QuitApp))
        {
            window->stop = true;
        }

        window->events.clear();

        FrameMark
    }
    render_sample_destroy(app);
    return 0;
}
