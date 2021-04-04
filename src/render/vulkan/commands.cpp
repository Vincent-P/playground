#include "render/vulkan/commands.hpp"
#include "render/vulkan/descriptor_set.hpp"
#include "render/vulkan/device.hpp"

#include "render/vulkan/queues.hpp"
#include "render/vulkan/surface.hpp"
#include "render/vulkan/utils.hpp"
#include "vulkan/vulkan_core.h"

namespace vulkan
{

/// --- Work

void Work::begin()
{
    VkCommandBufferBeginInfo binfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    binfo.flags                    = 0;
    vkBeginCommandBuffer(command_buffer, &binfo);

    bind_global_set();
}

void Work::bind_global_set()
{
    if (queue_type == QueueType::Graphics)
    {
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, device->global_set.vkpipelinelayout, 0, 1, &device->global_set.vkset, 1, &device->global_set.dynamic_offset);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, device->global_set.vkpipelinelayout, 0, 1, &device->global_set.vkset, 1, &device->global_set.dynamic_offset);
    }
    else if (queue_type == QueueType::Compute)
    {
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, device->global_set.vkpipelinelayout, 0, 1, &device->global_set.vkset, 1, &device->global_set.dynamic_offset);
    }
}

void Work::end()
{
    VK_CHECK(vkEndCommandBuffer(command_buffer));
}

void Work::wait_for(Fence &fence, u64 wait_value, VkPipelineStageFlags stage_dst)
{
    wait_fence_list.push_back(fence);
    wait_value_list.push_back(wait_value);
    wait_stage_list.push_back(stage_dst);
}

void Work::wait_for_acquired(Surface &surface, VkPipelineStageFlags stage_dst)
{
    image_acquired_semaphore = surface.image_acquired_semaphores[surface.previous_image];
    image_acquired_stage = stage_dst;
}

void Work::prepare_present(Surface &surface)
{
    signal_present_semaphore = surface.can_present_semaphores[surface.current_image];
}

void Work::barrier(Handle<Buffer> buffer_handle, BufferUsage usage_destination)
{
    auto &buffer = *device->buffers.get(buffer_handle);

    auto src_access = get_src_buffer_access(buffer.usage);
    auto dst_access = get_dst_buffer_access(usage_destination);
    auto b          = get_buffer_barrier(buffer.vkhandle, src_access, dst_access, 0, buffer.desc.size);
    vkCmdPipelineBarrier(command_buffer, src_access.stage, dst_access.stage, 0, 0, nullptr, 1, &b, 0, nullptr);

    buffer.usage = usage_destination;
}

void Work::barrier(Handle<Image> image_handle, ImageUsage usage_destination)
{
    auto &image = *device->images.get(image_handle);

    auto src_access = get_src_image_access(image.usage);
    auto dst_access = get_dst_image_access(usage_destination);
    auto b          = get_image_barrier(image.vkhandle, src_access, dst_access, image.full_range);
    vkCmdPipelineBarrier(command_buffer, src_access.stage, dst_access.stage, 0, 0, nullptr, 0, nullptr, 1, &b);

    image.usage = usage_destination;
}

void Work::clear_barrier(Handle<Image> image_handle, ImageUsage usage_destination)
{
    auto &image = *device->images.get(image_handle);

    auto src_access = get_src_image_access(ImageUsage::None);
    auto dst_access = get_dst_image_access(usage_destination);
    auto b          = get_image_barrier(image.vkhandle, src_access, dst_access, image.full_range);
    vkCmdPipelineBarrier(command_buffer, src_access.stage, dst_access.stage, 0, 0, nullptr, 0, nullptr, 1, &b);

    image.usage = usage_destination;
}

void Work::barriers(Vec<std::pair<Handle<Image>, ImageUsage>> images, Vec<std::pair<Handle<Buffer>, BufferUsage>> buffers)
{
    Vec<VkImageMemoryBarrier> image_barriers;
    Vec<VkBufferMemoryBarrier> buffer_barriers;

    VkPipelineStageFlags src_stage = 0;
    VkPipelineStageFlags dst_stage = 0;

    for (auto &[image_handle, usage_dst] : images)
    {
        auto &image = *device->images.get(image_handle);
        auto src_access = get_src_image_access(image.usage);
        auto dst_access = get_dst_image_access(usage_dst);
        image_barriers.push_back(get_image_barrier(image.vkhandle, src_access, dst_access, image.full_range));
        src_stage |= src_access.stage;
        dst_stage |= dst_access.stage;

        image.usage = usage_dst;
    }

    for (auto &[buffer_handle, usage_dst] : buffers)
    {
        auto &buffer = *device->buffers.get(buffer_handle);
        auto src_access = get_src_buffer_access(buffer.usage);
        auto dst_access = get_dst_buffer_access(usage_dst);
        buffer_barriers.push_back(get_buffer_barrier(buffer.vkhandle, src_access, dst_access, 0, buffer.desc.size));
        src_stage |= src_access.stage;
        dst_stage |= dst_access.stage;

        buffer.usage = usage_dst;
    }

    vkCmdPipelineBarrier(command_buffer, src_stage, dst_stage, 0, 0, nullptr, buffer_barriers.size(), buffer_barriers.data(), image_barriers.size(), image_barriers.data());
}

/// --- TransferWork
void TransferWork::copy_buffer(Handle<Buffer> src, Handle<Buffer> dst, Vec<std::pair<u32, u32>> offsets_sizes)
{
    auto &src_buffer = *device->buffers.get(src);
    auto &dst_buffer = *device->buffers.get(dst);

    Vec<VkBufferCopy> buffer_copies(offsets_sizes.size());
    for (usize i_copy = 0; i_copy < offsets_sizes.size(); i_copy += 1)
    {
        buffer_copies[i_copy] = {
            .srcOffset = offsets_sizes[i_copy].first,
            .dstOffset = offsets_sizes[i_copy].first,
            .size      = offsets_sizes[i_copy].second,
        };
    }

    vkCmdCopyBuffer(command_buffer, src_buffer.vkhandle, dst_buffer.vkhandle, buffer_copies.size(), buffer_copies.data());
}

void TransferWork::copy_buffer(Handle<Buffer> src, Handle<Buffer> dst)
{
    auto &src_buffer = *device->buffers.get(src);
    auto &dst_buffer = *device->buffers.get(dst);

    VkBufferCopy copy = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size      = std::min(src_buffer.desc.size, dst_buffer.desc.size),
    };

    vkCmdCopyBuffer(command_buffer, src_buffer.vkhandle, dst_buffer.vkhandle, 1, &copy);
}

void TransferWork::copy_buffer_to_image(Handle<Buffer> src, Handle<Image> dst)
{
    auto &src_buffer = *device->buffers.get(src);
    auto &dst_image  = *device->images.get(dst);

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;

    // If either of these values is zero, that aspect of the buffer memory is considered to be tightly packed according to the imageExtent.
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.mipLevel = dst_image.full_range.baseMipLevel;
    region.imageSubresource.aspectMask = dst_image.full_range.aspectMask;
    region.imageSubresource.layerCount = dst_image.full_range.layerCount;
    region.imageSubresource.baseArrayLayer = dst_image.full_range.baseArrayLayer;
    region.imageExtent.width = dst_image.desc.size.x;
    region.imageExtent.height = dst_image.desc.size.y;
    region.imageExtent.depth = dst_image.desc.size.z;

    vkCmdCopyBufferToImage(command_buffer, src_buffer.vkhandle, dst_image.vkhandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void TransferWork::fill_buffer(Handle<Buffer> buffer_handle, u32 data)
{
    auto &buffer = *device->buffers.get(buffer_handle);
    vkCmdFillBuffer(command_buffer, buffer.vkhandle, 0, buffer.desc.size, data);
}
/// --- ComputeWork

void ComputeWork::bind_pipeline(Handle<ComputeProgram> program_handle)
{
    auto &program = *device->compute_programs.get(program_handle);
    VkDescriptorSet set = find_or_create_descriptor_set(*device, program.descriptor_set);

    Vec<u32> offsets;
    offsets.reserve(program.descriptor_set.dynamic_offsets.size());
    offsets.insert(offsets.end(), program.descriptor_set.dynamic_offsets.begin(), program.descriptor_set.dynamic_offsets.end());

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, program.pipeline_layout, 1, 1, &set, offsets.size(), offsets.data());
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, program.pipeline);
}

void ComputeWork::dispatch(uint3 workgroups)
{
    vkCmdDispatch(command_buffer, workgroups.x, workgroups.y, workgroups.z);
}

void ComputeWork::clear_image(Handle<Image> image_handle, VkClearColorValue clear_color)
{
    auto image = *device->images.get(image_handle);

    vkCmdClearColorImage(command_buffer, image.vkhandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &image.full_range);
}

void ComputeWork::bind_uniform_buffer(Handle<ComputeProgram> program_handle, u32 slot, Handle<Buffer> buffer_handle, usize offset, usize size)
{
    auto &program = *device->compute_programs.get(program_handle);
    auto &buffer = *device->buffers.get(buffer_handle);
    assert(offset + size < buffer.desc.size);
    ::vulkan::bind_uniform_buffer(program.descriptor_set, slot, buffer_handle, offset, size);
}

void ComputeWork::bind_uniform_buffer(Handle<GraphicsProgram> program_handle, u32 slot, Handle<Buffer> buffer_handle, usize offset, usize size)
{
    auto &program = *device->graphics_programs.get(program_handle);
    auto &buffer = *device->buffers.get(buffer_handle);
    assert(offset + size < buffer.desc.size);
    ::vulkan::bind_uniform_buffer(program.descriptor_set, slot, buffer_handle, offset, size);
}

void ComputeWork::bind_storage_buffer(Handle<ComputeProgram> program_handle, u32 slot, Handle<Buffer> buffer_handle)
{
    auto &program = *device->compute_programs.get(program_handle);
    ::vulkan::bind_storage_buffer(program.descriptor_set, slot, buffer_handle);
}

void ComputeWork::bind_storage_buffer(Handle<GraphicsProgram> program_handle, u32 slot, Handle<Buffer> buffer_handle)
{
    auto &program = *device->graphics_programs.get(program_handle);
    ::vulkan::bind_storage_buffer(program.descriptor_set, slot, buffer_handle);
}

void ComputeWork::bind_storage_image(Handle<ComputeProgram> program_handle, u32 slot, Handle<Image> image_handle)
{
    auto &program = *device->compute_programs.get(program_handle);
    ::vulkan::bind_image(program.descriptor_set, slot, image_handle);
}

void ComputeWork::bind_storage_image(Handle<GraphicsProgram> program_handle, u32 slot, Handle<Image> image_handle)
{
    auto &program = *device->graphics_programs.get(program_handle);
    ::vulkan::bind_image(program.descriptor_set, slot, image_handle);
}

void ComputeWork::push_constant(Handle<GraphicsProgram> program_handle, void *data, usize len)
{
    auto &program = *device->graphics_programs.get(program_handle);
    vkCmdPushConstants(command_buffer, program.pipeline_layout, VK_SHADER_STAGE_ALL, 0, len, data);
}

/// --- GraphicsWork


void GraphicsWork::draw_indexed(const DrawIndexedOptions &options)
{
    vkCmdDrawIndexed(command_buffer, options.vertex_count, options.instance_count, options.index_offset, options.vertex_offset, options.instance_offset);
}

void GraphicsWork::set_scissor(const VkRect2D &rect)
{
    vkCmdSetScissor(command_buffer, 0, 1, &rect);
}

void GraphicsWork::set_viewport(const VkViewport &viewport)
{
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
}

void GraphicsWork::begin_pass(Handle<RenderPass> renderpass_handle, Handle<Framebuffer> framebuffer_handle, Vec<Handle<Image>> attachments, Vec<VkClearValue> clear_values)
{
    auto &renderpass = *device->renderpasses.get(renderpass_handle);
    auto &framebuffer = *device->framebuffers.get(framebuffer_handle);

    Vec<VkImageView> views(attachments.size());
    for (u32 i_attachment = 0; i_attachment < attachments.size(); i_attachment++)
    {
        views[i_attachment] = device->images.get(attachments[i_attachment])->full_view;
    }

    VkRenderPassAttachmentBeginInfo attachments_info = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO };
    attachments_info.attachmentCount = views.size();
    attachments_info.pAttachments = views.data();

    VkRenderPassBeginInfo begin_info = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    begin_info.pNext = &attachments_info;
    begin_info.renderPass = renderpass.vkhandle;
    begin_info.framebuffer = framebuffer.vkhandle;
    begin_info.renderArea.extent.width = framebuffer.desc.width;
    begin_info.renderArea.extent.height = framebuffer.desc.height;
    begin_info.clearValueCount = clear_values.size();
    begin_info.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void GraphicsWork::end_pass()
{
    vkCmdEndRenderPass(command_buffer);
}

void GraphicsWork::bind_pipeline(Handle<GraphicsProgram> program_handle, uint pipeline_index)
{
    auto &program = *device->graphics_programs.get(program_handle);
    auto pipeline = program.pipelines[pipeline_index];

    VkDescriptorSet set = find_or_create_descriptor_set(*device, program.descriptor_set);

    Vec<u32> offsets;
    offsets.reserve(program.descriptor_set.dynamic_offsets.size());
    offsets.insert(offsets.end(), program.descriptor_set.dynamic_offsets.begin(), program.descriptor_set.dynamic_offsets.end());

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, program.pipeline_layout, 1, 1, &set, offsets.size(), offsets.data());
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

void GraphicsWork::bind_index_buffer(Handle<Buffer> buffer_handle, VkIndexType index_type)
{
    auto &buffer = *device->buffers.get(buffer_handle);
    vkCmdBindIndexBuffer(command_buffer, buffer.vkhandle, 0, index_type);
}

/// --- Device

// WorkPool
void Device::create_work_pool(WorkPool &work_pool)
{
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = 0,
        .queueFamilyIndex = this->graphics_family_idx,
    };

    VK_CHECK(vkCreateCommandPool(this->device, &pool_info, nullptr, &work_pool.graphics().vk_handle));

    pool_info.queueFamilyIndex = this->compute_family_idx;
    VK_CHECK(vkCreateCommandPool(this->device, &pool_info, nullptr, &work_pool.compute().vk_handle));

    pool_info.queueFamilyIndex = this->transfer_family_idx;
    VK_CHECK(vkCreateCommandPool(this->device, &pool_info, nullptr, &work_pool.transfer().vk_handle));
}

void Device::reset_work_pool(WorkPool &work_pool)
{
    for (auto &command_pool : work_pool.command_pools)
    {
        if (!command_pool.free_list.empty()) {
            vkFreeCommandBuffers(this->device, command_pool.vk_handle, command_pool.free_list.size(), command_pool.free_list.data());
        }
        command_pool.free_list.clear();

        VK_CHECK(vkResetCommandPool(this->device, command_pool.vk_handle, 0));
    }
}

void Device::destroy_work_pool(WorkPool &work_pool)
{
    for (auto &command_pool : work_pool.command_pools)
    {
        vkDestroyCommandPool(device, command_pool.vk_handle, nullptr);
    }
}

// Work
static Work create_work(Device &device, WorkPool &work_pool, QueueType queue_type)
{
    auto &command_pool = work_pool.command_pools[to_underlying(queue_type)];

    Work work = {};
    work.device = &device;

    VkCommandBufferAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool                 = command_pool.vk_handle;
    ai.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount          = 1;
    VK_CHECK(vkAllocateCommandBuffers(device.device, &ai, &work.command_buffer));

    u32 queue_family_idx =
        queue_type == QueueType::Graphics ? device.graphics_family_idx
        : queue_type == QueueType::Compute  ? device.compute_family_idx
        : queue_type == QueueType::Transfer ? device.transfer_family_idx
        : u32_invalid;

    assert(queue_family_idx != u32_invalid);
    vkGetDeviceQueue(device.device, queue_family_idx, 0, &work.queue);

    work.queue_type = queue_type;

    command_pool.free_list.push_back(work.command_buffer);

    return work;
}

GraphicsWork Device::get_graphics_work(WorkPool &work_pool)
{
    return {{{{create_work(*this, work_pool, QueueType::Graphics)}}}};
}

ComputeWork Device::get_compute_work(WorkPool &work_pool)
{
    return {{{create_work(*this, work_pool, QueueType::Compute)}}};
}

TransferWork Device::get_transfer_work(WorkPool &work_pool)
{
    return {{create_work(*this, work_pool, QueueType::Transfer)}};
}

// Fences
Fence Device::create_fence(u64 initial_value)
{
    Fence fence = {};
    VkSemaphoreTypeCreateInfo timeline_info =  {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
    timeline_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timeline_info.initialValue = initial_value;

    VkSemaphoreCreateInfo semaphore_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    semaphore_info.pNext = &timeline_info;

    VK_CHECK(vkCreateSemaphore(device, &semaphore_info, nullptr, &fence.timeline_semaphore));

    return fence;
}

u64 Device::get_fence_value(Fence &fence)
{
    VK_CHECK(vkGetSemaphoreCounterValue(device, fence.timeline_semaphore, &fence.value));
    return fence.value;
}

void Device::set_fence_value(Fence &fence, u64 value)
{
    VkSemaphoreSignalInfo signal_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO};
    signal_info.semaphore = fence.timeline_semaphore;
    signal_info.value = value;

    VK_CHECK(vkSignalSemaphore(device, &signal_info));
}

void Device::destroy_fence(Fence &fence)
{
    vkDestroySemaphore(device, fence.timeline_semaphore, nullptr);
    fence.timeline_semaphore = VK_NULL_HANDLE;
}

// Submission
void Device::submit(Work &work, const Vec<Fence> &signal_fences, const Vec<u64> &signal_values)
{
    // Creathe list of semaphores to wait
    Vec<VkSemaphore> signal_list;
    signal_list.reserve(signal_fences.size() + 1);
    Vec<u64> local_signal_values = signal_values;
    for (const auto &fence : signal_fences)
    {
        signal_list.push_back(fence.timeline_semaphore);
    }

    if (work.signal_present_semaphore)
    {
        signal_list.push_back(work.signal_present_semaphore.value());
        local_signal_values.push_back(0);
    }

    Vec<VkSemaphore> semaphore_list;
    Vec<u64> value_list;
    Vec<VkPipelineStageFlags> stage_list;

    semaphore_list.reserve(work.wait_fence_list.size() + 1);
    value_list.reserve(work.wait_fence_list.size() + 1);
    stage_list.reserve(work.wait_fence_list.size() + 1);

    for (uint i = 0; i < work.wait_fence_list.size(); i++)
    {
        semaphore_list.push_back(work.wait_fence_list[i].timeline_semaphore);
        value_list.push_back(work.wait_value_list[i]);
        stage_list.push_back(work.wait_stage_list[i]);
    }

    // vulkan hacks
    if (work.image_acquired_semaphore)
    {
        semaphore_list.push_back(work.image_acquired_semaphore.value());
        value_list.push_back(0);
        stage_list.push_back(work.image_acquired_stage.value());
    }

    VkTimelineSemaphoreSubmitInfo timeline_info = {.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    timeline_info.waitSemaphoreValueCount = value_list.size();
    timeline_info.pWaitSemaphoreValues = value_list.data();
    timeline_info.signalSemaphoreValueCount = local_signal_values.size();
    timeline_info.pSignalSemaphoreValues = local_signal_values.data();

    VkSubmitInfo submit_info            = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit_info.pNext                   = &timeline_info;
    submit_info.waitSemaphoreCount      = semaphore_list.size();
    submit_info.pWaitSemaphores         = semaphore_list.data();
    submit_info.pWaitDstStageMask       = stage_list.data();
    submit_info.commandBufferCount      = 1;
    submit_info.pCommandBuffers         = &work.command_buffer;
    submit_info.signalSemaphoreCount    = signal_list.size();
    submit_info.pSignalSemaphores       = signal_list.data();

    VK_CHECK(vkQueueSubmit(work.queue, 1, &submit_info, VK_NULL_HANDLE));
}

bool Device::present(Surface &surface, Work &work)
{
    VkPresentInfoKHR present_i   = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present_i.waitSemaphoreCount = 1;
    present_i.pWaitSemaphores    = &surface.can_present_semaphores[surface.current_image];
    present_i.swapchainCount     = 1;
    present_i.pSwapchains        = &surface.swapchain;
    present_i.pImageIndices      = &surface.current_image;

    auto res = vkQueuePresentKHR(work.queue, &present_i);

    if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return true;
    }
    else
    {
        VK_CHECK(res);
    }

    return false;
}

void Device::wait_for_fence(const Fence &fence, u64 wait_value)
{
    // 10 sec in nanoseconds
    u64 timeout = 10llu*1000llu*1000llu*1000llu;
    VkSemaphoreWaitInfo wait_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
    wait_info.semaphoreCount = 1;
    wait_info.pSemaphores = &fence.timeline_semaphore;
    wait_info.pValues = &wait_value;
    VK_CHECK(vkWaitSemaphores(device, &wait_info, timeout));
}

void Device::wait_for_fences(const Vec<Fence> &fences, const Vec<u64> &wait_values)
{
    assert(wait_values.size() == fences.size());

    Vec<VkSemaphore> semaphores(fences.size());
    for (uint i_fence = 0; i_fence < fences.size(); i_fence += 1)
    {
        semaphores[i_fence] = fences[i_fence].timeline_semaphore;
    }

    // 10 sec in nanoseconds
    u64 timeout = 10llu*1000llu*1000llu*1000llu;
    VkSemaphoreWaitInfo wait_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
    wait_info.semaphoreCount = fences.size();
    wait_info.pSemaphores = semaphores.data();
    wait_info.pValues = wait_values.data();
    VK_CHECK(vkWaitSemaphores(device, &wait_info, timeout));
}

void Device::wait_idle()
{
    VK_CHECK(vkDeviceWaitIdle(device));
}

bool Device::acquire_next_swapchain(Surface &surface)
{
    bool error = false;

    surface.previous_image = surface.current_image;

    auto res = vkAcquireNextImageKHR(
        device,
        surface.swapchain,
        std::numeric_limits<uint64_t>::max(),
        surface.image_acquired_semaphores[surface.current_image],
        nullptr,
        &surface.current_image);

    if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
    {
        error = true;
    }
    else
    {
        VK_CHECK(res);
    }

    return error;
}
}
