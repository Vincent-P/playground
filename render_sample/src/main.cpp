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

PACKED(struct FontVertex
{
    exo::int2 position;
    exo::float2 uv;
})

PACKED (struct PushConstants
{
    u32 draw_id        = u32_invalid;
    u32 gui_texture_id = u32_invalid;
})

struct RenderSample
{
    exo::Window *window;
    Inputs inputs;
    BaseRenderer *renderer;

    FT_Library   library; /* handle to library     */
    FT_Face      face;    /* handle to face object */
    hb_font_t   *font = nullptr;
    hb_buffer_t *buf  = nullptr;
    Vec<u8>      current_file_data;

    GlyphCache *glyph_cache;

    ImGuiPass imgui_pass = {};
    Handle<gfx::GraphicsProgram>                                           font_program;
    Handle<gfx::Buffer> glyph_buffer;
    Handle<gfx::Image> font_atlas;
    exo::DynamicArray<Handle<gfx::Framebuffer>, gfx::MAX_SWAPCHAIN_IMAGES> swapchain_framebuffers;
};

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
    UI::new_frame();

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

    return app;
}

void render_sample_destroy(RenderSample *app)
{
    ZoneScoped;

    hb_buffer_destroy(app->buf);
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
    const u8 *text = app->current_file_data.data();

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
        UI::new_frame();
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
    UI::new_frame();

    {
    ZoneScopedN("Text rendering");
    unsigned int         glyph_count;
    hb_glyph_info_t     *glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_count);
    hb_glyph_position_t *glyph_pos  = hb_buffer_get_glyph_positions(buf, &glyph_count);

    auto [p_vertices, vert_offset] = renderer.dynamic_vertex_buffer.allocate(renderer.device, glyph_count * 4 * sizeof(FontVertex));
    auto first_vertex              = static_cast<u32>(vert_offset / sizeof(FontVertex));
    auto *vertices                 = reinterpret_cast<FontVertex *>(p_vertices);

    auto [p_indices, ind_offset] = renderer.dynamic_index_buffer.allocate(renderer.device, glyph_count * 6 * sizeof(u32));
    auto *indices                = reinterpret_cast<u32 *>(p_indices);

    hb_position_t cursor_x = 50;
    int           line_height = face->size->metrics.height >> 6;
    hb_position_t cursor_y    = 50 + line_height;
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
            ZoneScopedN("Upload glyph");

            int error = 0;
            {
                ZoneScopedN("Load glyph");
                error = FT_Load_Glyph(face, glyph_index, 0);
                ASSERT(!error);
            }

            {
                ZoneScopedN("Render glyph");
                error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
                ASSERT(!error);
            }

            FT_GlyphSlot  slot = face->glyph;

            auto scope = exo::ScopeStack::with_allocator(&exo::tls_allocator);

            ASSERT(slot->bitmap.width <= 16);
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

            const ImageRegion regions[] = {{.image_offset = exo::int2{(int)cache_entry.x * 16, (int)cache_entry.y * 20}, .image_size   = exo::int2{upload_width, upload_height}}};
            bool uploaded = renderer.streamer.upload_image_regions(app->font_atlas, tmp_buffer, upload_width * upload_height, regions);

            if (uploaded)
            {
                cache_entry.uploaded = true;
                cache_entry.glyph_top_left = {slot->bitmap_left, slot->bitmap_top};
                cache_entry.glyph_size     = {static_cast<int>(slot->bitmap.width), static_cast<int>(slot->bitmap.rows)};
            }
        }

        auto glyph_top_left = cache_entry.glyph_top_left;
        auto glyph_size = cache_entry.glyph_size;

        if (cursor_x + glyph_size.x > window->size.x || text[glyph_info[i].cluster] == '\n')
        {
            cursor_x = 50;
            cursor_y += line_height;
        }

        if (glyph_size.x == 0 || glyph_size.y == 0)
        {
            indices[6 * i + 0] = 0; // top left
            indices[6 * i + 1] = 0; // bottom left
            indices[6 * i + 2] = 0; // top right
            indices[6 * i + 3] = 0; // top right
            indices[6 * i + 4] = 0; // bottom left
            indices[6 * i + 5] = 0; // bottom right
            continue;
        }

        exo::int2 top_left;
        top_left.x = +glyph_top_left.x;
        top_left.y = -glyph_top_left.y;

        exo::int2 top_right;
        top_right.x = top_left.x + glyph_size.x;
        top_right.y = top_left.y;

        exo::int2 bottom_left;
        bottom_left.x = top_left.x;
        bottom_left.y = top_left.y + glyph_size.y;

        exo::int2 bottom_right;
        bottom_right.x = top_left.x + glyph_size.x;
        bottom_right.y = top_left.y + glyph_size.y;

        exo::int2 cursor = {cursor_x, cursor_y};

        vertices[4 * i + 0].position = cursor + top_left;
        vertices[4 * i + 0].uv       = {(cache_entry.x * 16) / 4096.0f, (cache_entry.y * 20) / 4096.0f};
        vertices[4 * i + 1].position = cursor + bottom_left;
        vertices[4 * i + 1].uv       = {(cache_entry.x * 16) / 4096.0f, (cache_entry.y * 20 + glyph_size.y) / 4096.0f};
        vertices[4 * i + 2].position = cursor + top_right;
        vertices[4 * i + 2].uv       = {(cache_entry.x * 16 + glyph_size.x) / 4096.0f, (cache_entry.y * 20) / 4096.0f};
        vertices[4 * i + 3].position = cursor + bottom_right;
        vertices[4 * i + 3].uv       = {(cache_entry.x * 16 + glyph_size.x) / 4096.0f, (cache_entry.y * 20 + glyph_size.y) / 4096.0f};

        indices[6 * i + 0] = first_vertex + 4 * i + 0; // top left
        indices[6 * i + 1] = first_vertex + 4 * i + 1; // bottom left
        indices[6 * i + 2] = first_vertex + 4 * i + 2; // top right

        indices[6 * i + 3] = first_vertex + 4 * i + 2; // top right
        indices[6 * i + 4] = first_vertex + 4 * i + 1; // bottom left
        indices[6 * i + 5] = first_vertex + 4 * i + 3; // bottom right

        cursor_x += x_advance >> 6;
        cursor_y += y_advance >> 6;
    }

    PACKED(struct FontOptions {
        float2 scale;
        float2 translation;
        u32    vertices_descriptor_index;
        u32    font_atlas_descriptor_index;
    })

    auto *options = renderer.bind_shader_options<FontOptions>(cmd, font_program);
    std::memset(options, 0, sizeof(FontOptions));
    options->scale = float2(2.0f / window->size.x, 2.0f / window->size.y);
    options->translation = float2(-1.0f, -1.0f);
    options->vertices_descriptor_index = device.get_buffer_storage_index(renderer.dynamic_vertex_buffer.buffer);
    options->font_atlas_descriptor_index = device.get_image_sampled_index(app->font_atlas);

    cmd.barrier(app->font_atlas, gfx::ImageUsage::GraphicsShaderRead);
    cmd.barrier(surface.images[surface.current_image], gfx::ImageUsage::ColorAttachment);
    cmd.begin_pass(swapchain_framebuffer, std::array{gfx::LoadOp::load()});
    VkViewport viewport{};
    viewport.width    = window->size.x;
    viewport.height   = window->size.y;
    viewport.minDepth = 1.0f;
    viewport.maxDepth = 1.0f;
    cmd.set_viewport(viewport);

    VkRect2D scissor;
    scissor.offset.x      = 0;
    scissor.offset.y      = 0;
    scissor.extent.width  = window->size.x;
    scissor.extent.height = window->size.y;
    cmd.set_scissor(scissor);

    cmd.bind_pipeline(font_program, 0);
    cmd.bind_index_buffer(renderer.dynamic_index_buffer.buffer, VK_INDEX_TYPE_UINT32, ind_offset);

    u32 constants[] = {0, 0};
    cmd.push_constant(constants, sizeof(constants));
    cmd.draw_indexed({.vertex_count  = glyph_count * 6});

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

        render(app);

        FrameMark
    }
    render_sample_destroy(app);
    return 0;
}
