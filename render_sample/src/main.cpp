#include <exo/prelude.h>

#include <exo/logger.h>
#include <exo/memory/linear_allocator.h>
#include <exo/memory/scope_stack.h>
#include <exo/collections/dynamic_array.h>
#include <exo/os/window.h>

#include <engine/render/vulkan/context.h>
#include <engine/render/vulkan/device.h>
#include <engine/render/vulkan/surface.h>
#include <engine/render/base_renderer.h>
#include <engine/render/imgui_pass.h>
namespace gfx = vulkan;

#include <engine/inputs.h>
#include <engine/ui.h>

#include <Tracy.hpp>
#include <array>
#include <imgui.h>

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

PACKED (struct PushConstants
{
    u32 draw_id        = u32_invalid;
    u32 gui_texture_id = u32_invalid;
})

static void resize(gfx::Device &device, gfx::Surface &surface,
                   exo::DynamicArray<Handle<gfx::Framebuffer>, gfx::MAX_SWAPCHAIN_IMAGES> &framebuffers)
{
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

static void render(BaseRenderer &renderer, exo::DynamicArray<Handle<gfx::Framebuffer>, gfx::MAX_SWAPCHAIN_IMAGES> &framebuffers, ImGuiPass &imgui_pass)
{
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
        renderer.streamer.upload(imgui_pass.font_atlas, pixels, width * height * sizeof(u32));
    }
    renderer.streamer.update(work_pool);

    auto swapchain_framebuffer = framebuffers[surface.current_image];

    cmd.clear_barrier(surface.images[surface.current_image], gfx::ImageUsage::ColorAttachment);
    cmd.begin_pass(swapchain_framebuffer, std::array{gfx::LoadOp::clear({.color = {.float32 = {1.0f, 1.0f, 0.0f, 0.0f}}})});
    cmd.end_pass();

    ImGui::Text("Frame: %zu", renderer.frame_count);

    ImGui::Render();
    if (renderer.streamer.is_uploaded(imgui_pass.font_atlas))
    {
        imgui_pass_draw(renderer, imgui_pass, cmd, swapchain_framebuffer);
    }
    UI::new_frame();

    cmd.barrier(surface.images[surface.current_image], gfx::ImageUsage::Present);

    cmd.end();

    out_of_date_swapchain = renderer.end_frame(cmd);
    if (out_of_date_swapchain)
    {
        resize(device, surface, framebuffers);
    }
}

u8  global_stack_mem[64 << 10];
int main(int /*argc*/, char ** /*argv*/)
{
    exo::LinearAllocator global_allocator = exo::LinearAllocator::with_external_memory(global_stack_mem, sizeof(global_stack_mem));
    exo::ScopeStack global_scope = exo::ScopeStack::with_allocator(&global_allocator);

    auto *window = exo::Window::create(global_scope, 1280, 720, "Render sample");
    Inputs inputs = {};
    inputs.bind(Action::QuitApp, {.keys = {exo::VirtualKey::Escape}});

    auto *renderer = BaseRenderer::create(global_scope,
                                                   window,
                                                   {
                                                       .push_constant_layout  = {.size = sizeof(PushConstants)},
                                                       .buffer_device_address = false,
                                                   });

    exo::DynamicArray<Handle<gfx::Framebuffer>, gfx::MAX_SWAPCHAIN_IMAGES> swapchain_fb = {};
    swapchain_fb.resize(renderer->surface.images.size());
    resize(renderer->device, renderer->surface, swapchain_fb);

    ImGuiPass imgui_pass = {};
    UI::create_context(window, &inputs);
    imgui_pass_init(renderer->device, imgui_pass, renderer->surface.format.format);
    UI::new_frame();

    while (!window->should_close())
    {
        window->poll_events();

        Option<exo::events::Resize> last_resize  = {};
        bool                       is_minimized = window->minimized;
        for (const auto &event : window->events)
        {
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

        render(*renderer, swapchain_fb, imgui_pass);

        FrameMark
    }

    return 0;
}
