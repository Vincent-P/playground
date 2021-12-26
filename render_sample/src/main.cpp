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

struct UiButton
{
    Rect rect;
};

struct Painter
{
    u8 *vertices;
    PrimitiveIndex *indices;

    usize vertices_size;
    usize indices_size;
    usize vertex_bytes_offset;
    u32 index_offset;
};

struct UiState
{
    u64 focused;
    u64 active;
};

PACKED(struct PushConstants {
    u32 draw_id        = u32_invalid;
    u32 gui_texture_id = u32_invalid;
})

struct RenderSample
{
    exo::Window *window;
    Inputs inputs;
    BaseRenderer *renderer;
    Painter *painter;

    FT_Library   library; /* handle to library     */
    FT_Face      face;    /* handle to face object */
    hb_font_t   *font = nullptr;
    hb_buffer_t *buf  = nullptr;
    Vec<u8>      current_file_data;
    int cursor_offset = 0;

    GlyphCache *glyph_cache;

    ImGuiPass imgui_pass = {};
    Handle<gfx::GraphicsProgram>                                           font_program;
    Handle<gfx::Buffer> glyph_buffer;
    Handle<gfx::Image> font_atlas;
    exo::DynamicArray<Handle<gfx::Framebuffer>, gfx::MAX_SWAPCHAIN_IMAGES> swapchain_framebuffers;
};

// --- fwd
void open_file(RenderSample *app, const std::filesystem::path &path);

// --- UI

static Painter *painter_allocate(exo::ScopeStack &scope, usize vertex_buffer_size, usize index_buffer_size)
{
    auto *painter = scope.allocate<Painter>();
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

    app->font_atlas = renderer->device.create_image({
        .name   = "Monochrome font atlas",
        .size   = {4096, 4096, 1},
        .format = VK_FORMAT_R8_UNORM,
    });

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

    error = FT_New_Face(app->library, "C:\\Windows\\Fonts\\JetBrainsMono-Regular.ttf", 0, &app->face);
    ASSERT (!error);
    FT_Set_Char_Size(app->face, 0, 13*64, 0, 96);

    app->font = hb_ft_font_create_referenced(app->face);
    hb_ft_font_set_funcs(app->font);
    app->buf = hb_buffer_create();

    app->glyph_cache = GlyphCache::create(scope, {.hash_count = 64 << 10, .entry_count = (4096 / 16) * (4096 / 20), .glyph_per_row = 4096 / 16});

    app->painter = painter_allocate(scope, 16_MiB, 16_MiB);

    return app;
}

void render_sample_destroy(RenderSample *app)
{
    ZoneScoped;

    hb_buffer_destroy(app->buf);
}

static bool upload_glyph(RenderSample *app, u32 glyph_index, i32 x, i32 y)
{
    ZoneScopedN("Upload glyph");

    // Render the glyph with FreeType
    int error = 0;
    {
        ZoneScopedN("Load glyph");
        error = FT_Load_Glyph(app->face, glyph_index, 0);
        ASSERT(!error);
    }

    {
        ZoneScopedN("Render glyph");
        error = FT_Render_Glyph(app->face->glyph, FT_RENDER_MODE_NORMAL);
        ASSERT(!error);
    }

    FT_GlyphSlot  slot = app->face->glyph;

    // Copy the bitmap to a temporary upload buffer
    auto scope = exo::ScopeStack::with_allocator(&exo::tls_allocator);
    int upload_width = 16;
    int upload_height = ((slot->bitmap.rows / 16) + 1) * 16;
    u8 *tmp_buffer = reinterpret_cast<u8*>(scope.allocate(upload_width * upload_height));
    std::memset(tmp_buffer, 0, upload_width * upload_height);
    for (usize i_row = 0; i_row < slot->bitmap.rows; i_row += 1)
    {
        for (usize i_col = 0; i_col < slot->bitmap.width; i_col += 1)
        {
            tmp_buffer[i_row * upload_width + i_col] = slot->bitmap.buffer[i_row * slot->bitmap.pitch + i_col];
        }
    }

    // Upload it to GPU
    const ImageRegion regions[] = {{.image_offset = exo::int2{x * 16, y * 20}, .image_size   = exo::int2{upload_width, upload_height}}};
    return app->renderer->streamer.upload_image_regions(app->font_atlas, tmp_buffer, upload_width * upload_height, regions);
}

static void display_ui(RenderSample *app)
{
    static char input_command_buffer[128] = "Hello, world!";
    if (ImGui::InputText("Command", input_command_buffer, sizeof(input_command_buffer), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        exo::logger::info("Command: {}\n", input_command_buffer);
        std::memset(input_command_buffer, 0, sizeof(input_command_buffer));
    }

    if (ImGui::Button("Open file"))
    {
        if (auto fs_path = exo::file_dialog({{"file", "*"}}))
        {
            open_file(app, fs_path.value());
        }
    }

    ImGui::Text("Current file size: %zu", app->current_file_data.size());

    // -- Painter
    app->painter->index_offset = 0;
    app->painter->vertex_bytes_offset = 0;

    const u8 *text = app->current_file_data.data();
    unsigned int         glyph_count;
    hb_glyph_info_t     *glyph_info = hb_buffer_get_glyph_infos(app->buf, &glyph_count);
    hb_glyph_position_t *glyph_pos  = hb_buffer_get_glyph_positions(app->buf, &glyph_count);
    int           line_height = app->face->size->metrics.height >> 6;

    hb_position_t cursor_x = 50;
    hb_position_t cursor_y    = app->cursor_offset + 50 + line_height;
    for (unsigned int i = 0; i < glyph_count; i++)
    {
        hb_codepoint_t glyph_index   = glyph_info[i].codepoint;
        hb_position_t  x_offset  = glyph_pos[i].x_offset;
        hb_position_t  y_offset  = glyph_pos[i].y_offset;
        hb_position_t  x_advance = glyph_pos[i].x_advance;
        hb_position_t  y_advance = glyph_pos[i].y_advance;

        auto &cache_entry = app->glyph_cache->get_or_create(glyph_index);

        if (cache_entry.uploaded == false)
        {
            if (upload_glyph(app, glyph_index, cache_entry.x, cache_entry.y))
            {
                FT_GlyphSlot slot          = app->face->glyph;
                cache_entry.uploaded       = true;
                cache_entry.glyph_top_left = {slot->bitmap_left, slot->bitmap_top};
                cache_entry.glyph_size = {static_cast<int>(slot->bitmap.width), static_cast<int>(slot->bitmap.rows)};
            }
        }

        if (cursor_x + cache_entry.glyph_size.x > app->window->size.x)
        {
            cursor_x = 50;
            cursor_y += line_height;
        }

        Rect rect = {
            .position = exo::float2(exo::int2{cursor_x + cache_entry.glyph_top_left.x, cursor_y - cache_entry.glyph_top_left.y}),
            .size = exo::float2(cache_entry.glyph_size),
        };
        Rect uv = {
            .position = {(cache_entry.x * 16) / 4096.0f, (cache_entry.y * 20) / 4096.0f},
            .size = {cache_entry.glyph_size.x / 4096.0f, cache_entry.glyph_size.y / 4096.0f}
        };
        painter_draw_textured_rect(*app->painter, rect, uv, app->renderer->device.get_image_sampled_index(app->font_atlas));

        cursor_x += x_advance >> 6;
        cursor_y += y_advance >> 6;

        if (text[glyph_info[i].cluster] == '\n')
        {
            cursor_x = 50;
            cursor_y += line_height;
        }
    }
}

static void render(RenderSample *app)
{
    ZoneScoped;

    auto &renderer = *app->renderer;
    auto & framebuffers = app->swapchain_framebuffers;
    auto &imgui_pass = app->imgui_pass;
    auto *buf = app->buf;
    auto &face = app->face;
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

        cmd.clear_barrier(app->font_atlas, gfx::ImageUsage::TransferDst);
        cmd.clear_image(app->font_atlas, VkClearColorValue{.float32 = {0.0f, 0.0f, 0.0f, 0.0f}});
        cmd.barrier(app->font_atlas, gfx::ImageUsage::GraphicsShaderRead);
    }
    renderer.streamer.update(cmd);

    auto swapchain_framebuffer = framebuffers[surface.current_image];

    cmd.clear_barrier(surface.images[surface.current_image], gfx::ImageUsage::ColorAttachment);
    cmd.begin_pass(swapchain_framebuffer, std::array{gfx::LoadOp::clear({.color = {.float32 = {1.0f, 1.0f, 0.0f, 0.0f}}})});
    cmd.end_pass();

    ImGui::Text("Frame: %u", renderer.frame_count);
    ImGui::Image((void*)(u64)renderer.device.get_image_sampled_index(app->font_atlas), exo::float2(256, 256));

    ImGui::Render();
    imgui_pass_draw(renderer, imgui_pass, cmd, swapchain_framebuffer);

    {
    auto *painter = app->painter;
    ZoneScopedN("Painter");
    auto [p_vertices, vert_offset] = renderer.dynamic_vertex_buffer.allocate(renderer.device, painter->vertex_bytes_offset, sizeof(TexturedRect));
    ASSERT(vert_offset % sizeof(TexturedRect) == 0);
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

    cmd.barrier(app->font_atlas, gfx::ImageUsage::GraphicsShaderRead);
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

void open_file(RenderSample *app, const std::filesystem::path &path)
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

    hb_buffer_clear_contents(app->buf);
    hb_buffer_add_utf8(app->buf, reinterpret_cast<const char *>(app->current_file_data.data()), app->current_file_data.size(), 0, -1);
    hb_buffer_set_direction(app->buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(app->buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(app->buf, hb_language_from_string("en", -1));

    hb_shape(app->font, app->buf, nullptr, 0);
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

    // window->user_data = app;
    // window->paint_callback = +[](void *user_data) {
    //     auto *app = reinterpret_cast<RenderSample*>(user_data);
    //     render(app);
    // };

    while (!window->should_close())
    {
        window->poll_events();

        Option<exo::events::Resize> last_resize  = {};
        bool                       is_minimized = window->minimized;
        for (const auto &event : window->events)
        {
            auto &io = ImGui::GetIO();
            switch (event.type)
            {
            case exo::Event::ResizeType:
            {
                last_resize = event.resize;
                break;
            }
            case exo::Event::MouseMoveType:
            {
                is_minimized = false;
                break;
            }
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
        UI::new_frame();

        if (auto scroll = inputs.get_scroll_this_frame())
        {
            app->cursor_offset -= scroll->y * 7;
        }
        if (inputs.is_pressed(Action::QuitApp))
        {
            window->stop = true;
        }

        window->events.clear();
        if (window->should_close())
        {
            break;
        }
        if (is_minimized)
        {
            continue;
        }

        display_ui(app);

        render(app);

        FrameMark
    }
    render_sample_destroy(app);
    return 0;
}
