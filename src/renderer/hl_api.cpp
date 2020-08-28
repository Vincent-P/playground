#include "renderer/hl_api.hpp"
#include "renderer/vlk_context.hpp"
#include <iostream>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace my_app::vulkan
{

API API::create(const Window &window)
{
    API api;
    api.ctx = Context::create(window);

    api.swapchain_usages.resize(api.ctx.swapchain.images.size());
    for (auto &swapchain_usage : api.swapchain_usages) {
        swapchain_usage = ImageUsage::None;
    }

    {
	BufferInfo binfo;
	binfo.name                  = "Staging Buffer";
	binfo.size                  = 16 * 1024 * 1024;
	binfo.usage                 = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	binfo.memory_usage          = VMA_MEMORY_USAGE_CPU_TO_GPU;
	api.staging_buffer.buffer_h = api.create_buffer(binfo);
	api.staging_buffer.offset   = 0;
    }

    {
	BufferInfo binfo;
	binfo.name                     = "Dynamic Vertex Buffer";
	binfo.size                     = 64 * 1024 * 1024;
	binfo.usage                    = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	binfo.memory_usage             = VMA_MEMORY_USAGE_CPU_TO_GPU;
	api.dyn_vertex_buffer.buffer_h = api.create_buffer(binfo);
	api.dyn_vertex_buffer.offset   = 0;
    }

    {
	BufferInfo binfo;
	binfo.name                      = "Dynamic Uniform Buffer";
	binfo.size                      = 64 * 1024 * 1024;
	binfo.usage                     = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	binfo.memory_usage              = VMA_MEMORY_USAGE_CPU_TO_GPU;
	api.dyn_uniform_buffer.buffer_h = api.create_buffer(binfo);
	api.dyn_uniform_buffer.offset   = 0;
    }

    {
	BufferInfo binfo;
	binfo.name                    = "Dynamic Index Buffer";
	binfo.size                    = 16 * 1024 * 1024;
	binfo.usage                   = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	binfo.memory_usage            = VMA_MEMORY_USAGE_CPU_TO_GPU;
	api.dyn_index_buffer.buffer_h = api.create_buffer(binfo);
	api.dyn_index_buffer.offset   = 0;
    }

    {
        api.timestamp_labels_per_frame.resize(FRAMES_IN_FLIGHT);
        api.cpu_timestamps_per_frame.resize(FRAMES_IN_FLIGHT);
    }

    api.swapchain_rth = api.create_rendertarget({.is_swapchain = true});

    return api;
}

void API::destroy()
{
    for (auto& fb : framebuffers)
    {
        vkDestroyFramebuffer(ctx.device, fb.vkhandle, nullptr);
    }

    for (auto& rp : renderpasses)
    {
        vkDestroyRenderPass(ctx.device, rp.vkhandle, nullptr);
    }

    for (auto& [buffer_h, buffer] : buffers)
    {
        destroy_buffer_internal(*this, *buffer);
    }

    for (auto& [image_h, image] : images)
    {
        destroy_image_internal(*this, *image);
    }

    for (auto& [sampler_h, sampler] : samplers)
    {
        destroy_sampler_internal(*this, *sampler);
    }

    for (auto& [program_h, program] : graphics_programs)
    {
        destroy_program_internal(*this, *program);
    }

    for (auto& [program_h, program] : compute_programs)
    {
        destroy_program_internal(*this, *program);
    }

    for (auto& [shader_h, shader] : shaders)
    {
        destroy_shader_internal(*this, *shader);
    }

    ctx.destroy();
}

void API::on_resize(int width, int height)
{
    // submit all command buffers?

    ctx.on_resize(width, height);
    swapchain_usages.resize(ctx.swapchain.images.size());
    for (auto &swapchain_usage : swapchain_usages) {
        swapchain_usage = ImageUsage::None;
    }

    for (auto &timestamps : timestamp_labels_per_frame) {
        timestamps.clear();
    }
}

bool API::start_frame()
{
    auto &frame_resource = ctx.frame_resources.get_current();

    // TODO: user defined unit for time and timer for profiling
    auto wait_result = vkWaitForFences(ctx.device, 1, &frame_resource.fence, true, 10lu * 1000lu * 1000lu * 1000lu);
    if (wait_result == VK_TIMEOUT) {
	throw std::runtime_error("Submitted the frame more than 10 second ago.");
    }
    VK_CHECK(wait_result);

    // Reset the current frame
    VK_CHECK(vkResetFences(ctx.device, 1, &frame_resource.fence));

    vkFreeCommandBuffers(ctx.device, frame_resource.command_pool, 1, &frame_resource.command_buffer);
    VK_CHECK(vkResetCommandPool(ctx.device, frame_resource.command_pool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT));

    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool        = frame_resource.command_pool;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(ctx.device, &ai, &frame_resource.command_buffer));


    /// --- Read timestamp from previous frame and copy them in cpu memory

    uint frame_idx = ctx.frame_count % FRAMES_IN_FLIGHT;
    auto &current_timestamp_labels = timestamp_labels_per_frame[frame_idx];
    auto &cpu_timestamps = cpu_timestamps_per_frame[frame_idx];
    usize timestamps_to_read = current_timestamp_labels.size();

    // there is at least one timestamp to read
    if (timestamps_to_read != 0)
    {

        // read gpu timestamps
        std::array<u64, MAX_TIMESTAMP_PER_FRAME> gpu_timestamps;
        // timestampPeriod is the number of nanoseconds per timestamp value increment
        double ms_per_tick = (1e-3f * ctx.physical_props.limits.timestampPeriod);
        VK_CHECK(vkGetQueryPoolResults(ctx.device, ctx.timestamp_pool, frame_idx * MAX_TIMESTAMP_PER_FRAME , timestamps_to_read, timestamps_to_read * sizeof(u64), gpu_timestamps.data(), sizeof(u64), VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));

        // copy the timestamps to display them later in the frame
        timestamps.resize(timestamps_to_read);
        for (usize i = 0; i < timestamps_to_read; i++)
        {
            timestamps[i] = {
                .label = current_timestamp_labels[i],
                .gpu_microseconds = float(ms_per_tick * (gpu_timestamps[i] - gpu_timestamps[0])),
                .cpu_milliseconds = elapsed_ms<float>(cpu_timestamps[0], cpu_timestamps[i]),
            };
        }

        current_timestamp_labels.clear();
    }

    vkResetQueryPool(ctx.device, ctx.timestamp_pool, frame_idx * MAX_TIMESTAMP_PER_FRAME, MAX_TIMESTAMP_PER_FRAME);
    cpu_timestamps.clear();

    VkCommandBufferBeginInfo binfo{};
    binfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    binfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(frame_resource.command_buffer, &binfo);

    add_timestamp("Begin Frame");

    return true;
}

bool API::start_present()
{
    auto &frame_resource = ctx.frame_resources.get_current();

    auto res = vkAcquireNextImageKHR(ctx.device, ctx.swapchain.handle,
                                    std::numeric_limits<uint64_t>::max(),
                                    frame_resource.image_available,
                                    nullptr,
                                   &ctx.swapchain.current_image);

    if (res == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return false;
    }

    if (res != VK_SUBOPTIMAL_KHR)
    {
        VK_CHECK(res);
    }

    return true;
}

void API::end_frame()
{
    add_timestamp("End Frame");

    auto &frame_resource = ctx.frame_resources.get_current();
    auto cmd = frame_resource.command_buffer;

    /// --- Transition swapchain to present
    auto &swapchain_usage = swapchain_usages[ctx.swapchain.current_image];
    assert(swapchain_usage == ImageUsage::ColorAttachment);

    VkImage vkimage = ctx.swapchain.get_current_image();
    VkImageSubresourceRange range{
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
    };

    auto src = vulkan::get_src_image_access(swapchain_usage);
    auto dst = vulkan::get_dst_image_access(vulkan::ImageUsage::Present);
    auto b   = vulkan::get_image_barrier(vkimage, src, dst, range);
    vkCmdPipelineBarrier(cmd, src.stage, dst.stage, 0, 0, nullptr, 0, nullptr, 1, &b);
    swapchain_usage = vulkan::ImageUsage::Present;

    /// --- Submit command buffer
    VkQueue graphics_queue;
    vkGetDeviceQueue(ctx.device, ctx.graphics_family_idx, 0, &graphics_queue);

    VK_CHECK(vkEndCommandBuffer(frame_resource.command_buffer));

    VkPipelineStageFlags stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{};
    si.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = &frame_resource.image_available;
    si.pWaitDstStageMask    = &stage;
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &frame_resource.command_buffer;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = &frame_resource.rendering_finished;

    VK_CHECK(vkQueueSubmit(graphics_queue, 1, &si, frame_resource.fence));

    /// --- Present the frame
    VkPresentInfoKHR present_i{};
    present_i.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_i.waitSemaphoreCount = 1;
    present_i.pWaitSemaphores    = &frame_resource.rendering_finished;
    present_i.swapchainCount     = 1;
    present_i.pSwapchains        = &ctx.swapchain.handle;
    present_i.pImageIndices      = &ctx.swapchain.current_image;

    auto res = vkQueuePresentKHR(graphics_queue, &present_i);

    if (res != VK_SUBOPTIMAL_KHR)
    {
        VK_CHECK(res);
    }

    ctx.frame_count += 1;
    ctx.frame_resources.current = ctx.frame_count % ctx.frame_resources.data.size();
    vmaSetCurrentFrameIndex(ctx.allocator, ctx.frame_count);
}

void API::wait_idle() { VK_CHECK(vkDeviceWaitIdle(ctx.device)); }

} // namespace my_app::vulkan
