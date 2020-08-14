#include "renderer/hl_api.hpp"
#include "renderer/vlk_context.hpp"
#include <iostream>
#include <vulkan/vulkan.hpp>

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
	binfo.name                      = "Dynamic Uniform Buffer";
	binfo.size                      = 64 * 1024 * 1024;
	binfo.usage                     = vk::BufferUsageFlagBits::eUniformBuffer;
	binfo.memory_usage              = VMA_MEMORY_USAGE_CPU_TO_GPU;
	api.dyn_uniform_buffer.buffer_h = api.create_buffer(binfo);
	api.dyn_uniform_buffer.offset   = 0;
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

    {
        api.current_timestamp_labels.resize(FRAMES_IN_FLIGHT);
    }

    return api;
}

void API::destroy()
{
    for (auto& buffer : buffers)
    {
        destroy_buffer_internal(*this, buffer);
    }

    for (auto& image : images)
    {
        destroy_image_internal(*this, image);
    }

    ctx.destroy();
}

void API::on_resize(int width, int height)
{
    // submit all command buffers?

    ctx.on_resize(width, height);
}

bool API::start_frame()
{
    auto &frame_resource = ctx.frame_resources.get_current();

    // TODO: user defined unit for time and timer for profiling
    auto wait_result = ctx.device->waitForFences(*frame_resource.fence, VK_TRUE, 10lu * 1000lu * 1000lu * 1000lu);
    if (wait_result == vk::Result::eTimeout) {
	throw std::runtime_error("Submitted the frame more than 10 second ago.");
    }

    // Reset the current frame
    ctx.device->resetFences(frame_resource.fence.get());
    ctx.device->resetCommandPool(*frame_resource.command_pool, {vk::CommandPoolResetFlagBits::eReleaseResources});
    frame_resource.command_buffer = std::move(ctx.device->allocateCommandBuffersUnique(
	{*frame_resource.command_pool, vk::CommandBufferLevel::ePrimary, 1})[0]);

    // TODO: check compile and work, add timestamp during begin_label?
    // there is at least one timestamp to read
    uint frame_idx = ctx.frame_count % FRAMES_IN_FLIGHT;
    usize timestamps_to_read = current_timestamp_labels.size();
    if (timestamps_to_read != 0)
    {
        // timestampPeriod is the number of nanoseconds per timestamp value increment
        double ms_per_tick = (1e-3f * ctx.physical_props.limits.timestampPeriod);

        std::array<u64, MAX_TIMESTAMP_PER_FRAME> gpu_timestamps;
        auto res = ctx.device->getQueryPoolResults(*ctx.timestamp_pool, frame_idx * MAX_TIMESTAMP_PER_FRAME, timestamps_to_read, timestamps_to_read * sizeof(u64), gpu_timestamps.data(), sizeof(u64), vk::QueryResultFlagBits::eWithAvailability);
        (void)(res);
        assert(res == vk::Result::eSuccess);

        timestamps.resize(timestamps_to_read);
        for (usize i = 0; i < timestamps_to_read; i++)
        {
            timestamps[i] = {.label = current_timestamp_labels[i], .microseconds = float(ms_per_tick * (gpu_timestamps[i] - gpu_timestamps[0]))};
        }

        current_timestamp_labels.clear();
    }

    ctx.device->resetQueryPool(*ctx.timestamp_pool, frame_idx * MAX_TIMESTAMP_PER_FRAME, MAX_TIMESTAMP_PER_FRAME);


    // TODO: dont acquire swapchain image yet, make separate start_present to acquire it only for post process
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

    vk::CommandBufferBeginInfo binfo{};
    frame_resource.command_buffer->begin(binfo);
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
    catch (const std::exception &) {
	return;
    }

    ctx.frame_count += 1;
    ctx.frame_resources.current = ctx.frame_count % ctx.frame_resources.data.size();
    vmaSetCurrentFrameIndex(ctx.allocator, ctx.frame_count);
}

void API::wait_idle() { ctx.device->waitIdle(); }

} // namespace my_app::vulkan
