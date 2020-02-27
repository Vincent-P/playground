#include "renderer/hl_api.hpp"
#include <iostream>

namespace my_app::vulkan
{

API API::create(const Window &window)
{
    API api;
    api.ctx = Context::create(window);

    {
        BufferInfo binfo;
        binfo.name                  = "Staging Buffer";
        binfo.size                  = 16 * 1024 * 1024;
        binfo.usage                 = vk::BufferUsageFlagBits::eTransferSrc;
        binfo.memory_usage          = VMA_MEMORY_USAGE_CPU_TO_GPU;
        api.staging_buffer.buffer_h = api.create_buffer(binfo);
        api.staging_buffer.offset   = 0;
    }

    {
        BufferInfo binfo;
        binfo.name                     = "Dynamic Vertex Buffer";
        binfo.size                     = 64 * 1024 * 1024;
        binfo.usage                    = vk::BufferUsageFlagBits::eVertexBuffer;
        binfo.memory_usage             = VMA_MEMORY_USAGE_CPU_TO_GPU;
        api.dyn_vertex_buffer.buffer_h = api.create_buffer(binfo);
        api.dyn_vertex_buffer.offset   = 0;
    }

    {
        BufferInfo binfo;
        binfo.name                    = "Dynamic Index Buffer";
        binfo.size                    = 16 * 1024 * 1024;
        binfo.usage                   = vk::BufferUsageFlagBits::eIndexBuffer;
        binfo.memory_usage            = VMA_MEMORY_USAGE_CPU_TO_GPU;
        api.dyn_index_buffer.buffer_h = api.create_buffer(binfo);
        api.dyn_index_buffer.offset   = 0;
    }

    return api;
}

void API::destroy()
{
    destroy_buffer(staging_buffer.buffer_h);
    destroy_buffer(dyn_vertex_buffer.buffer_h);
    destroy_buffer(dyn_index_buffer.buffer_h);

    ctx.destroy();
}

void API::on_resize(int width, int height)
{
    // submit all command buffers
    ctx.on_resize(width, height);
}

bool API::start_frame()
{
    auto &frame_resource = ctx.frame_resources.get_current();

    // TODO: user defined unit?
    auto wait_result = ctx.device->waitForFences(*frame_resource.fence, VK_TRUE, 10lu * 1000lu * 1000lu * 1000lu);
    if (wait_result == vk::Result::eTimeout) {
        throw std::runtime_error("Submitted the frame more than 10 second ago.");
    }

    // Reset the current frame
    ctx.device->resetFences(frame_resource.fence.get());
    ctx.device->resetCommandPool(*frame_resource.command_pool, {vk::CommandPoolResetFlagBits::eReleaseResources});
    frame_resource.command_buffer = std::move(ctx.device->allocateCommandBuffersUnique(
        {*frame_resource.command_pool, vk::CommandBufferLevel::ePrimary, 1})[0]);

    auto result
        = ctx.device->acquireNextImageKHR(*ctx.swapchain.handle, std::numeric_limits<uint64_t>::max(),
                                          *frame_resource.image_available, nullptr, &ctx.swapchain.current_image);

    if (result == vk::Result::eErrorOutOfDateKHR) {
#if 0
        std::cerr << "The swapchain is out of date. Recreating it...\n";
        ctx.destroy_swapchain();
        ctx.create_swapchain();
        start_frame();
        return;
#endif
	return false;
    }

    frame_resource.command_buffer->begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    return true;
}

void API::end_frame()
{
    auto graphics_queue = ctx.device->getQueue(ctx.graphics_family_idx, 0);

    auto &frame_resource = ctx.frame_resources.get_current();
    frame_resource.command_buffer->end();

    vk::PipelineStageFlags stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::SubmitInfo kernel{};
    kernel.waitSemaphoreCount   = 1;
    kernel.pWaitSemaphores      = &frame_resource.image_available.get();
    kernel.pWaitDstStageMask    = &stage;
    kernel.commandBufferCount   = 1;
    kernel.pCommandBuffers      = &frame_resource.command_buffer.get();
    kernel.signalSemaphoreCount = 1;
    kernel.pSignalSemaphores    = &frame_resource.rendering_finished.get();
    graphics_queue.submit(kernel, frame_resource.fence.get());

    // Present the frame
    vk::PresentInfoKHR present_i{};
    present_i.waitSemaphoreCount = 1;
    present_i.pWaitSemaphores    = &frame_resource.rendering_finished.get();
    present_i.swapchainCount     = 1;
    present_i.pSwapchains        = &ctx.swapchain.handle.get();
    present_i.pImageIndices      = &ctx.swapchain.current_image;

    try {
        graphics_queue.presentKHR(present_i);
    }
    catch(const std::exception&) {
	return;
    }
    ctx.frame_count += 1;
    ctx.frame_resources.current = ctx.frame_count % ctx.frame_resources.data.size();
}

void API::wait_idle() { ctx.device->waitIdle(); }

} // namespace my_app::vulkan
