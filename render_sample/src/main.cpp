#include <exo/base/logger.h>
#include <exo/memory/linear_allocator.h>
#include <exo/memory/scope_stack.h>
#include <exo/collections/dynamic_array.h>
#include <exo/cross/window.h>

#include <engine/render/vulkan/context.h>
#include <engine/render/vulkan/device.h>
#include <engine/render/vulkan/surface.h>
namespace gfx = vulkan;

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
                   DynamicArray<Handle<gfx::Framebuffer>, gfx::MAX_SWAPCHAIN_IMAGES> &framebuffers)
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

u8 global_stack_mem[64 << 10];
int main(int /*argc*/, char ** /*argv*/)
{
    LinearAllocator global_allocator = LinearAllocator::with_external_memory(global_stack_mem, sizeof(global_stack_mem));
    ScopeStack global_scope = ScopeStack::with_allocator(&global_allocator);

    auto *window = cross::Window::create(global_scope, 1280, 720, "Render sample");

    auto context = gfx::Context::create(false, window);

    auto &physical_devices = context.physical_devices;
    u32   i_selected       = u32_invalid;
    u32   i_device         = 0;
    for (auto &physical_device : physical_devices)
    {
        logger::info("Found device: {}\n", physical_device.properties.deviceName);
        if (i_device == u32_invalid && physical_device.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            logger::info("Prioritizing device {} because it is a discrete GPU.\n",
                         physical_device.properties.deviceName);
            i_selected = i_device;
        }
        i_device += 1;
    }
    if (i_selected == u32_invalid)
    {
        i_selected = 0;
        logger::info("No discrete GPU found, defaulting to device #0: {}.\n",
                     physical_devices[0].properties.deviceName);
    }

    auto device = gfx::Device::create(context,
                                      {
                                          .physical_device       = &physical_devices[i_selected],
                                          .push_constant_layout  = {.size = sizeof(u32) * 2},
                                          .buffer_device_address = false,
                                      });

    auto          surface   = gfx::Surface::create(context, device, *window);
    gfx::WorkPool work_pool = {};
    device.create_work_pool(work_pool);

    DynamicArray<Handle<gfx::Framebuffer>, gfx::MAX_SWAPCHAIN_IMAGES> swapchain_fb = {};
    swapchain_fb.resize(surface.images.size());
    resize(device, surface, swapchain_fb);

    while (!window->should_close())
    {
        window->poll_events();

        Option<cross::events::Resize> last_resize  = {};
        bool                          is_minimized = false;
        for (const auto &event : window->events)
        {
            switch (event.type)
            {
            case cross::Event::ResizeType:
            {
                last_resize = event.resize;
                break;
            }
            case cross::Event::MouseMoveType:
            {
                is_minimized = true;
                break;
            }
            case cross::Event::KeyType:
            {
                if (event.key.key == cross::VirtualKey::Escape)
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

        device.wait_idle();
        device.reset_work_pool(work_pool);

        bool out_of_date_swapchain = device.acquire_next_swapchain(surface);
        if (out_of_date_swapchain || (last_resize && last_resize->width > 0 && last_resize->height > 0))
        {
            resize(device, surface, swapchain_fb);
            continue;
        }

        gfx::GraphicsWork cmd = device.get_graphics_work(work_pool);
        cmd.begin();
        cmd.wait_for_acquired(surface, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        cmd.clear_barrier(surface.images[surface.current_image], gfx::ImageUsage::ColorAttachment);
        cmd.begin_pass(swapchain_fb[surface.current_image], std::array{gfx::LoadOp::clear({.color = {.float32 = {1.0f, 1.0f, 0.0f, 0.0f}}})});
        cmd.end_pass();
        cmd.barrier(surface.images[surface.current_image], gfx::ImageUsage::Present);

        cmd.prepare_present(surface);
        cmd.end();
        device.submit(cmd, {}, {});
        out_of_date_swapchain = device.present(surface, cmd);
        if (out_of_date_swapchain)
        {
            resize(device, surface, swapchain_fb);
        }

        FrameMark
    }

    device.wait_idle();
    for (usize i_image = 0; i_image < surface.images.size(); i_image += 1)
    {
        device.destroy_framebuffer(swapchain_fb[i_image]);
    }
    device.destroy_work_pool(work_pool);
    surface.destroy(context, device);
    device.destroy(context);
    context.destroy();

    return 0;
}
