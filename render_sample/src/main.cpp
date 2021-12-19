#include <exo/prelude.h>

#include <exo/logger.h>
#include <exo/memory/linear_allocator.h>
#include <exo/memory/scope_stack.h>
#include <exo/collections/dynamic_array.h>
#include <exo/os/window.h>

#include <engine/render/vulkan/context.h>
#include <engine/render/vulkan/device.h>
#include <engine/render/vulkan/surface.h>
namespace gfx = vulkan;
#include <engine/render/base_renderer.h>

#include <Tracy.hpp>
#include <array>

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

static void render(BaseRenderer &renderer, exo::DynamicArray<Handle<gfx::Framebuffer>, gfx::MAX_SWAPCHAIN_IMAGES> &framebuffers)
{
    auto &device = renderer.device;
    auto &surface = renderer.surface;
    auto &work_pool       = renderer.work_pools[renderer.frame_count & FRAME_QUEUE_LENGTH];

    device.wait_idle();
    device.reset_work_pool(work_pool);

    bool out_of_date_swapchain = device.acquire_next_swapchain(surface);
    if (out_of_date_swapchain)
    {
        resize(device, surface, framebuffers);
        return;
    }

    gfx::GraphicsWork cmd = device.get_graphics_work(work_pool);
    cmd.begin();
    cmd.wait_for_acquired(surface, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    cmd.clear_barrier(surface.images[surface.current_image], gfx::ImageUsage::ColorAttachment);
    cmd.begin_pass(framebuffers[surface.current_image],
                   std::array{gfx::LoadOp::clear({.color = {.float32 = {1.0f, 1.0f, 0.0f, 0.0f}}})});
    cmd.end_pass();
    cmd.barrier(surface.images[surface.current_image], gfx::ImageUsage::Present);

    cmd.prepare_present(surface);
    cmd.end();
    device.submit(cmd, {}, {});
    out_of_date_swapchain = device.present(surface, cmd);
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


    auto *renderer = BaseRenderer::create(global_scope,
                                                   window,
                                                   {
                                                       .push_constant_layout  = {.size = 2 * sizeof(u32)},
                                                       .buffer_device_address = false,
                                                   });

    exo::DynamicArray<Handle<gfx::Framebuffer>, gfx::MAX_SWAPCHAIN_IMAGES> swapchain_fb = {};
    swapchain_fb.resize(renderer->surface.images.size());
    resize(renderer->device, renderer->surface, swapchain_fb);

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
            case exo::Event::KeyType:
            {
                if (event.key.key == exo::VirtualKey::Escape)
                {
                    window->stop = true;
                }
                break;
            }
            default:
                break;
            }
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

        render(*renderer, swapchain_fb);

        FrameMark
    }

    return 0;
}
