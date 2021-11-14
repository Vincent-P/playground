#include "render/vulkan/commands.h"
#include "render/vulkan/descriptor_set.h"
#include "render/vulkan/device.h"

#include "render/vulkan/pipelines.h"
#include "render/vulkan/queries.h"
#include "render/vulkan/queues.h"
#include "render/vulkan/surface.h"
#include "render/vulkan/utils.h"
#include "vulkan/vulkan_core.h"
#include <exo/collections/dynamic_array.h>

namespace vulkan
{

/// --- Work

void Work::begin()
{
    ZoneScoped;
    VkCommandBufferBeginInfo binfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    binfo.flags                    = 0;
    vkBeginCommandBuffer(command_buffer, &binfo);
}

void Work::bind_global_set()
{
    ZoneScoped;
    if (queue_type == QueueType::Graphics || queue_type == QueueType::Compute)
    {
        VkPipelineLayout layout = device->global_sets.pipeline_layout;
        VkDescriptorSet  sets[] = {
            find_or_create_descriptor_set(*device, device->global_sets.uniform),
            device->global_sets.sampled_images.set,
            device->global_sets.storage_images.set,
            device->global_sets.storage_buffers.set,
        };

        u32      offsets_count = static_cast<u32>(device->global_sets.uniform.dynamic_offsets.size());
        DynamicArray<u32, MAX_DYNAMIC_DESCRIPTORS> offsets = {};
        for (usize i_offset = 0; i_offset < offsets_count; i_offset += 1)
        {
            offsets.push_back(static_cast<u32>(device->global_sets.uniform.dynamic_offsets[i_offset]));
        }

        if (queue_type == QueueType::Graphics)
        {
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, ARRAY_SIZE(sets), sets, offsets_count, offsets.data());
        }
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, ARRAY_SIZE(sets), sets, offsets_count, offsets.data());
    }
}

void Work::end()
{
    ZoneScoped;
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
    image_acquired_stage     = stage_dst;
}

void Work::prepare_present(Surface &surface)
{
    signal_present_semaphore = surface.can_present_semaphores[surface.current_image];
}

void Work::barrier(Handle<Buffer> buffer_handle, BufferUsage usage_destination)
{
    ZoneScoped;
    auto &buffer = *device->buffers.get(buffer_handle);

    auto src_access = get_src_buffer_access(buffer.usage);
    auto dst_access = get_dst_buffer_access(usage_destination);
    auto b          = get_buffer_barrier(buffer.vkhandle, src_access, dst_access, 0, buffer.desc.size);
    vkCmdPipelineBarrier(command_buffer, src_access.stage, dst_access.stage, 0, 0, nullptr, 1, &b, 0, nullptr);

    buffer.usage = usage_destination;
}

void Work::absolute_barrier(Handle<Image> image_handle)
{
    ZoneScoped;
    auto &image = *device->images.get(image_handle);

    auto src_access = get_src_image_access(image.usage);

    VkImageMemoryBarrier b = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.oldLayout            = src_access.layout;
    b.newLayout            = src_access.layout;
    b.srcAccessMask        = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    b.dstAccessMask        = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    b.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
    b.image                = image.vkhandle;
    b.subresourceRange     = image.full_view.range;

    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
}

void Work::barrier(Handle<Image> image_handle, ImageUsage usage_destination)
{
    ZoneScoped;
    auto &image = *device->images.get(image_handle);

    auto src_access = get_src_image_access(image.usage);
    auto dst_access = get_dst_image_access(usage_destination);
    auto b          = get_image_barrier(image.vkhandle, src_access, dst_access, image.full_view.range);
    vkCmdPipelineBarrier(command_buffer, src_access.stage, dst_access.stage, 0, 0, nullptr, 0, nullptr, 1, &b);

    image.usage = usage_destination;
}

void Work::clear_barrier(Handle<Image> image_handle, ImageUsage usage_destination)
{
    auto &image = *device->images.get(image_handle);

    auto src_access = get_src_image_access(ImageUsage::None);
    auto dst_access = get_dst_image_access(usage_destination);
    auto b          = get_image_barrier(image.vkhandle, src_access, dst_access, image.full_view.range);
    vkCmdPipelineBarrier(command_buffer, src_access.stage, dst_access.stage, 0, 0, nullptr, 0, nullptr, 1, &b);

    image.usage = usage_destination;
}

void Work::barriers(std::span<std::pair<Handle<Image>, ImageUsage>> images, std::span<std::pair<Handle<Buffer>, BufferUsage>> buffers)
{
    ZoneScoped;
    DynamicArray<VkImageMemoryBarrier, 8>  image_barriers = {};
    DynamicArray<VkBufferMemoryBarrier, 8> buffer_barriers = {};

    VkPipelineStageFlags src_stage = 0;
    VkPipelineStageFlags dst_stage = 0;

    for (auto &[image_handle, usage_dst] : images)
    {
        auto &image      = *device->images.get(image_handle);
        auto  src_access = get_src_image_access(image.usage);
        auto  dst_access = get_dst_image_access(usage_dst);
        image_barriers.push_back(get_image_barrier(image.vkhandle, src_access, dst_access, image.full_view.range));
        src_stage |= src_access.stage;
        dst_stage |= dst_access.stage;

        image.usage = usage_dst;
    }

    for (auto &[buffer_handle, usage_dst] : buffers)
    {
        auto &buffer     = *device->buffers.get(buffer_handle);
        auto  src_access = get_src_buffer_access(buffer.usage);
        auto  dst_access = get_dst_buffer_access(usage_dst);
        buffer_barriers.push_back(get_buffer_barrier(buffer.vkhandle, src_access, dst_access, 0, buffer.desc.size));
        src_stage |= src_access.stage;
        dst_stage |= dst_access.stage;

        buffer.usage = usage_dst;
    }

    vkCmdPipelineBarrier(command_buffer, src_stage, dst_stage, 0, 0, nullptr, static_cast<u32>(buffer_barriers.size()), buffer_barriers.data(), static_cast<u32>(image_barriers.size()), image_barriers.data());
}

// Queries
void Work::reset_query_pool(QueryPool &query_pool, u32 first_query, u32 count)
{
    ZoneScoped;
    vkCmdResetQueryPool(command_buffer, query_pool.vkhandle, first_query, count);
}

void Work::begin_query(QueryPool &query_pool, u32 index)
{
    ZoneScoped;
    vkCmdBeginQuery(command_buffer, query_pool.vkhandle, index, 0);
}

void Work::end_query(QueryPool &query_pool, u32 index)
{
    ZoneScoped;
    vkCmdEndQuery(command_buffer, query_pool.vkhandle, index);
}

void Work::timestamp_query(QueryPool &query_pool, u32 index)
{
    ZoneScoped;
    vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pool.vkhandle, index);
}

void Work::begin_debug_label(std::string_view label, float4 color)
{
    VkDebugUtilsLabelEXT label_info = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
    label_info.pLabelName = label.data();
    label_info.color[0] = color[0];
    label_info.color[1] = color[1];
    label_info.color[2] = color[2];
    label_info.color[3] = color[3];

    device->vkCmdBeginDebugUtilsLabelEXT(command_buffer, &label_info);
}

void Work::end_debug_label()
{
    device->vkCmdEndDebugUtilsLabelEXT(command_buffer);
}

/// --- TransferWork
void TransferWork::copy_buffer(Handle<Buffer> src, Handle<Buffer> dst, std::span<const std::tuple<usize, usize, usize>> offsets_src_dst_size)
{
    ZoneScoped;
    auto &src_buffer = *device->buffers.get(src);
    auto &dst_buffer = *device->buffers.get(dst);

    DynamicArray<VkBufferCopy, 16> buffer_copies = {};

    for (usize i_copy = 0; i_copy < offsets_src_dst_size.size(); i_copy += 1)
    {
        buffer_copies.push_back({
            .srcOffset = std::get<0>(offsets_src_dst_size[i_copy]),
            .dstOffset = std::get<1>(offsets_src_dst_size[i_copy]),
            .size      = std::get<2>(offsets_src_dst_size[i_copy]),
        });
    }

    vkCmdCopyBuffer(command_buffer, src_buffer.vkhandle, dst_buffer.vkhandle, static_cast<u32>(buffer_copies.size()), buffer_copies.data());
}

void TransferWork::copy_buffer(Handle<Buffer> src, Handle<Buffer> dst)
{
    ZoneScoped;
    auto &src_buffer = *device->buffers.get(src);
    auto &dst_buffer = *device->buffers.get(dst);

    VkBufferCopy copy = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size      = std::min(src_buffer.desc.size, dst_buffer.desc.size),
    };

    vkCmdCopyBuffer(command_buffer, src_buffer.vkhandle, dst_buffer.vkhandle, 1, &copy);
}

void TransferWork::copy_image(Handle<Image> src, Handle<Image> dst)
{
    ZoneScoped;
    auto &src_image = *device->images.get(src);
    auto &dst_image = *device->images.get(dst);

    VkImageCopy copy                   = {};
    copy.srcSubresource.aspectMask     = src_image.full_view.range.aspectMask;
    copy.srcSubresource.mipLevel       = src_image.full_view.range.baseMipLevel;
    copy.srcSubresource.baseArrayLayer = src_image.full_view.range.baseArrayLayer;
    copy.srcSubresource.layerCount     = src_image.full_view.range.layerCount;
    copy.srcOffset                     = {.x = 0, .y = 0, .z = 0};
    copy.dstSubresource.aspectMask     = dst_image.full_view.range.aspectMask;
    copy.dstSubresource.mipLevel       = dst_image.full_view.range.baseMipLevel;
    copy.dstSubresource.baseArrayLayer = dst_image.full_view.range.baseArrayLayer;
    copy.dstSubresource.layerCount     = dst_image.full_view.range.layerCount;
    copy.dstOffset                     = {.x = 0, .y = 0, .z = 0};
    copy.extent.width                  = static_cast<u32>(std::min(src_image.desc.size.x, dst_image.desc.size.x));
    copy.extent.height                 = static_cast<u32>(std::min(src_image.desc.size.y, dst_image.desc.size.y));
    copy.extent.depth                  = static_cast<u32>(std::min(src_image.desc.size.z, dst_image.desc.size.z));

    vkCmdCopyImage(command_buffer, src_image.vkhandle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_image.vkhandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
}

void TransferWork::blit_image(Handle<Image> src, Handle<Image> dst)
{
    ZoneScoped;
    auto &src_image = *device->images.get(src);
    auto &dst_image = *device->images.get(dst);

    VkImageBlit blit                   = {};
    blit.srcSubresource.aspectMask     = src_image.full_view.range.aspectMask;
    blit.srcSubresource.mipLevel       = src_image.full_view.range.baseMipLevel;
    blit.srcSubresource.baseArrayLayer = src_image.full_view.range.baseArrayLayer;
    blit.srcSubresource.layerCount     = src_image.full_view.range.layerCount;
    blit.srcOffsets[0]                 = {.x = 0, .y = 0, .z = 0};
    blit.srcOffsets[1]                 = {.x = static_cast<i32>(src_image.desc.size.x), .y = static_cast<i32>(src_image.desc.size.y), .z = static_cast<i32>(src_image.desc.size.z)};
    blit.dstSubresource.aspectMask     = dst_image.full_view.range.aspectMask;
    blit.dstSubresource.mipLevel       = dst_image.full_view.range.baseMipLevel;
    blit.dstSubresource.baseArrayLayer = dst_image.full_view.range.baseArrayLayer;
    blit.dstSubresource.layerCount     = dst_image.full_view.range.layerCount;
    blit.dstOffsets[0]                 = {.x = 0, .y = 0, .z = 0};
    blit.dstOffsets[1]                 = {.x = static_cast<i32>(dst_image.desc.size.x), .y = static_cast<i32>(dst_image.desc.size.y), .z = static_cast<i32>(dst_image.desc.size.z)};

    vkCmdBlitImage(command_buffer, src_image.vkhandle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_image.vkhandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
}

void TransferWork::copy_buffer_to_image(Handle<Buffer> src, Handle<Image> dst, std::span<VkBufferImageCopy> buffer_copy_regions)
{
    ZoneScoped;
    auto &src_buffer = *device->buffers.get(src);
    auto &dst_image  = *device->images.get(dst);

    u32 region_count = static_cast<u32>(buffer_copy_regions.size());
    vkCmdCopyBufferToImage(command_buffer, src_buffer.vkhandle, dst_image.vkhandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, region_count, buffer_copy_regions.data());
}

void TransferWork::fill_buffer(Handle<Buffer> buffer_handle, u32 data)
{
    ZoneScoped;
    auto &buffer = *device->buffers.get(buffer_handle);
    vkCmdFillBuffer(command_buffer, buffer.vkhandle, 0, buffer.desc.size, data);
}
/// --- ComputeWork

void ComputeWork::bind_pipeline(Handle<ComputeProgram> program_handle)
{
    ZoneScoped;
    auto &          program = *device->compute_programs.get(program_handle);
    VkDescriptorSet set     = find_or_create_descriptor_set(*device, program.descriptor_set);

    DynamicArray<u32, MAX_DYNAMIC_DESCRIPTORS> offsets = {};
    for (usize offset : program.descriptor_set.dynamic_offsets)
    {
        offsets.push_back(static_cast<u32>(offset));
    }

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, program.pipeline_layout, 4, 1, &set, static_cast<u32>(offsets.size()), offsets.data());
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, program.pipeline);
}

void ComputeWork::dispatch(uint3 workgroups)
{
    ZoneScoped;
    vkCmdDispatch(command_buffer, workgroups.x, workgroups.y, workgroups.z);
}

void ComputeWork::clear_image(Handle<Image> image_handle, VkClearColorValue clear_color)
{
    ZoneScoped;
    auto image = *device->images.get(image_handle);

    vkCmdClearColorImage(command_buffer, image.vkhandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &image.full_view.range);
}

void ComputeWork::bind_uniform_buffer(Handle<ComputeProgram> program_handle, u32 slot, Handle<Buffer> buffer_handle, usize offset, usize size)
{
    ZoneScoped;
    auto &program = *device->compute_programs.get(program_handle);
    auto &buffer  = *device->buffers.get(buffer_handle);
    ASSERT(offset + size < buffer.desc.size);
    ::vulkan::bind_uniform_buffer(program.descriptor_set, slot, buffer_handle, offset, size);
}

void ComputeWork::bind_uniform_buffer(Handle<GraphicsProgram> program_handle, u32 slot, Handle<Buffer> buffer_handle, usize offset, usize size)
{
    ZoneScoped;
    auto &program = *device->graphics_programs.get(program_handle);
    auto &buffer  = *device->buffers.get(buffer_handle);
    ASSERT(offset + size < buffer.desc.size);
    ::vulkan::bind_uniform_buffer(program.descriptor_set, slot, buffer_handle, offset, size);
}

void ComputeWork::bind_storage_buffer(Handle<ComputeProgram> program_handle, u32 slot, Handle<Buffer> buffer_handle)
{
    ZoneScoped;
    auto &program = *device->compute_programs.get(program_handle);
    ::vulkan::bind_storage_buffer(program.descriptor_set, slot, buffer_handle);
}

void ComputeWork::bind_storage_buffer(Handle<GraphicsProgram> program_handle, u32 slot, Handle<Buffer> buffer_handle)
{
    ZoneScoped;
    auto &program = *device->graphics_programs.get(program_handle);
    ::vulkan::bind_storage_buffer(program.descriptor_set, slot, buffer_handle);
}

void ComputeWork::bind_storage_image(Handle<ComputeProgram> program_handle, u32 slot, Handle<Image> image_handle)
{
    ZoneScoped;
    auto &program = *device->compute_programs.get(program_handle);
    ::vulkan::bind_image(program.descriptor_set, slot, image_handle);
}

void ComputeWork::bind_storage_image(Handle<GraphicsProgram> program_handle, u32 slot, Handle<Image> image_handle)
{
    ZoneScoped;
    auto &program = *device->graphics_programs.get(program_handle);
    ::vulkan::bind_image(program.descriptor_set, slot, image_handle);
}

void ComputeWork::push_constant(const void *data, usize len)
{
    ZoneScoped;
    vkCmdPushConstants(command_buffer, device->global_sets.pipeline_layout, VK_SHADER_STAGE_ALL, 0, static_cast<u32>(len), data);
}

/// --- GraphicsWork

void GraphicsWork::draw_indexed(const DrawIndexedOptions &options)
{
    ZoneScoped;
    vkCmdDrawIndexed(command_buffer, options.vertex_count, options.instance_count, options.index_offset, options.vertex_offset, options.instance_offset);
}

void GraphicsWork::draw(const DrawOptions &options)
{
    ZoneScoped;
    vkCmdDraw(command_buffer, options.vertex_count, options.instance_count, options.vertex_offset, options.instance_offset);
}

void GraphicsWork::draw_indexed_indirect_count(const DrawIndexedIndirectCountOptions &options)
{
    ZoneScoped;
    auto &arguments = *device->buffers.get(options.arguments_buffer);
    auto &count     = *device->buffers.get(options.count_buffer);
    vkCmdDrawIndexedIndirectCount(command_buffer, arguments.vkhandle, options.arguments_offset, count.vkhandle, options.count_offset, options.max_draw_count, options.stride);
}

void GraphicsWork::set_scissor(const VkRect2D &rect)
{
    ZoneScoped;
    vkCmdSetScissor(command_buffer, 0, 1, &rect);
}

void GraphicsWork::set_viewport(const VkViewport &viewport)
{
    ZoneScoped;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
}

void GraphicsWork::begin_pass(Handle<Framebuffer> framebuffer_handle, std::span<const LoadOp> load_ops)
{
    ZoneScoped;
    auto &framebuffer = *device->framebuffers.get(framebuffer_handle);
    auto &renderpass  = device->find_or_create_renderpass(framebuffer, load_ops);

    DynamicArray<VkClearValue, MAX_ATTACHMENTS> clear_colors = {};
    for (auto &load_op : load_ops)
    {
        clear_colors.push_back(load_op.color);
    }

    VkRenderPassBeginInfo begin_info    = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    begin_info.renderPass               = renderpass.vkhandle;
    begin_info.framebuffer              = framebuffer.vkhandle;
    begin_info.renderArea.extent.width  = static_cast<u32>(framebuffer.format.width);
    begin_info.renderArea.extent.height = static_cast<u32>(framebuffer.format.height);
    begin_info.clearValueCount          = static_cast<u32>(clear_colors.size());
    begin_info.pClearValues             = clear_colors.data();

    vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void GraphicsWork::end_pass()
{
    vkCmdEndRenderPass(command_buffer);
}

void GraphicsWork::bind_pipeline(Handle<GraphicsProgram> program_handle, uint pipeline_index)
{
    ZoneScoped;
    auto &program  = *device->graphics_programs.get(program_handle);
    auto  pipeline = program.pipelines[pipeline_index];

    VkDescriptorSet set = find_or_create_descriptor_set(*device, program.descriptor_set);

    DynamicArray<u32, MAX_DYNAMIC_DESCRIPTORS> offsets = {};
    for (usize offset : program.descriptor_set.dynamic_offsets)
    {
        offsets.push_back(static_cast<u32>(offset));
    }

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, program.pipeline_layout, 4, 1, &set, static_cast<u32>(offsets.size()), offsets.data());
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

void GraphicsWork::bind_index_buffer(Handle<Buffer> buffer_handle, VkIndexType index_type, usize offset)
{
    ZoneScoped;
    auto &buffer = *device->buffers.get(buffer_handle);
    vkCmdBindIndexBuffer(command_buffer, buffer.vkhandle, offset, index_type);
}

/// --- Device

// WorkPool
void Device::create_work_pool(WorkPool &work_pool)
{
    ZoneScoped;
    VkCommandPoolCreateInfo pool_info = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = 0,
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
    ZoneScoped;
    for (auto &command_pool : work_pool.command_pools)
    {
        if (!command_pool.free_list.empty())
        {
            vkFreeCommandBuffers(this->device, command_pool.vk_handle, static_cast<u32>(command_pool.free_list.size()), command_pool.free_list.data());
        }
        command_pool.free_list.clear();

        VK_CHECK(vkResetCommandPool(this->device, command_pool.vk_handle, 0));
    }
}

void Device::destroy_work_pool(WorkPool &work_pool)
{
    ZoneScoped;
    for (auto &command_pool : work_pool.command_pools)
    {
        vkDestroyCommandPool(device, command_pool.vk_handle, nullptr);
    }
}

// CommandPool
void Device::create_query_pool(QueryPool &query_pool, u32 query_capacity)
{
    ZoneScoped;
    VkQueryPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    pool_info.queryType             = VK_QUERY_TYPE_TIMESTAMP;
    pool_info.queryCount            = query_capacity;

    VK_CHECK(vkCreateQueryPool(device, &pool_info, nullptr, &query_pool.vkhandle));
}

void Device::reset_query_pool(QueryPool &query_pool, u32 first_query, u32 count)
{
    ZoneScoped;
    vkResetQueryPool(device, query_pool.vkhandle, first_query, count);
}

void Device::destroy_query_pool(QueryPool &query_pool)
{
    ZoneScoped;
    vkDestroyQueryPool(device, query_pool.vkhandle, nullptr);
    query_pool.vkhandle = VK_NULL_HANDLE;
}

void Device::get_query_results(QueryPool &query_pool, u32 first_query, u32 count, Vec<u64> &results)
{
    ZoneScoped;
    usize old_size = results.size();
    results.resize(old_size + count);

    vkGetQueryPoolResults(device, query_pool.vkhandle, first_query, count, count * sizeof(u64), results.data() + old_size, sizeof(u64), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
}

// Work
static Work create_work(Device &device, WorkPool &work_pool, QueueType queue_type)
{
    ZoneScoped;
    auto &command_pool = work_pool.command_pools[queue_type];

    Work work   = {};
    work.device = &device;

    VkCommandBufferAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool                 = command_pool.vk_handle;
    ai.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount          = 1;
    VK_CHECK(vkAllocateCommandBuffers(device.device, &ai, &work.command_buffer));

    u32 queue_family_idx = queue_type == QueueType::Graphics   ? device.graphics_family_idx
                           : queue_type == QueueType::Compute  ? device.compute_family_idx
                           : queue_type == QueueType::Transfer ? device.transfer_family_idx
                                                               : u32_invalid;

    ASSERT(queue_family_idx != u32_invalid);
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
    ZoneScoped;
    Fence                     fence         = {};
    VkSemaphoreTypeCreateInfo timeline_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
    timeline_info.semaphoreType             = VK_SEMAPHORE_TYPE_TIMELINE;
    timeline_info.initialValue              = initial_value;

    VkSemaphoreCreateInfo semaphore_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    semaphore_info.pNext                 = &timeline_info;

    VK_CHECK(vkCreateSemaphore(device, &semaphore_info, nullptr, &fence.timeline_semaphore));

    return fence;
}

u64 Device::get_fence_value(Fence &fence)
{
    ZoneScoped;
    VK_CHECK(vkGetSemaphoreCounterValue(device, fence.timeline_semaphore, &fence.value));
    return fence.value;
}

void Device::set_fence_value(Fence &fence, u64 value)
{
    ZoneScoped;
    VkSemaphoreSignalInfo signal_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO};
    signal_info.semaphore             = fence.timeline_semaphore;
    signal_info.value                 = value;

    VK_CHECK(vkSignalSemaphore(device, &signal_info));
}

void Device::destroy_fence(Fence &fence)
{
    ZoneScoped;
    vkDestroySemaphore(device, fence.timeline_semaphore, nullptr);
    fence.timeline_semaphore = VK_NULL_HANDLE;
}

// Submission
void Device::submit(Work &work, std::span<const Fence> signal_fences, std::span<const u64> signal_values)
{
    ZoneScoped;
    // Creathe list of semaphores to wait
    DynamicArray<VkSemaphore, 4> signal_list = {};
    DynamicArray<u64, 4> local_signal_values = {signal_values};
    for (const auto &fence : signal_fences)
    {
        signal_list.push_back(fence.timeline_semaphore);
    }

    if (work.signal_present_semaphore)
    {
        signal_list.push_back(work.signal_present_semaphore.value());
        local_signal_values.push_back(0);
    }

    DynamicArray<VkSemaphore, MAX_SEMAPHORES+1>          semaphore_list = {};
    DynamicArray<u64, MAX_SEMAPHORES+1>                  value_list = {};
    DynamicArray<VkPipelineStageFlags, MAX_SEMAPHORES+1> stage_list = {};

    for (usize i = 0; i < work.wait_fence_list.size(); i++)
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
    timeline_info.waitSemaphoreValueCount       = static_cast<u32>(value_list.size());
    timeline_info.pWaitSemaphoreValues          = value_list.data();
    timeline_info.signalSemaphoreValueCount     = static_cast<u32>(local_signal_values.size());
    timeline_info.pSignalSemaphoreValues        = local_signal_values.data();

    VkSubmitInfo submit_info         = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit_info.pNext                = &timeline_info;
    submit_info.waitSemaphoreCount   = static_cast<u32>(semaphore_list.size());
    submit_info.pWaitSemaphores      = semaphore_list.data();
    submit_info.pWaitDstStageMask    = stage_list.data();
    submit_info.commandBufferCount   = 1;
    submit_info.pCommandBuffers      = &work.command_buffer;
    submit_info.signalSemaphoreCount = static_cast<u32>(signal_list.size());
    submit_info.pSignalSemaphores    = signal_list.data();

    VK_CHECK(vkQueueSubmit(work.queue, 1, &submit_info, VK_NULL_HANDLE));
}

bool Device::present(Surface &surface, Work &work)
{
    ZoneScoped;
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
    ZoneScoped;
    // 10 sec in nanoseconds
    u64                 timeout   = 10llu * 1000llu * 1000llu * 1000llu;
    VkSemaphoreWaitInfo wait_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
    wait_info.semaphoreCount      = 1;
    wait_info.pSemaphores         = &fence.timeline_semaphore;
    wait_info.pValues             = &wait_value;
    VK_CHECK(vkWaitSemaphores(device, &wait_info, timeout));
}

void Device::wait_for_fences(std::span<const Fence> fences, std::span<const u64> wait_values)
{
    ZoneScoped;
    ASSERT(wait_values.size() == fences.size());

    DynamicArray<VkSemaphore, 4> semaphores = {};
    for (usize i_fence = 0; i_fence < fences.size(); i_fence += 1)
    {
        semaphores.push_back(fences[i_fence].timeline_semaphore);
    }

    // 10 sec in nanoseconds
    u64                 timeout   = 10llu * 1000llu * 1000llu * 1000llu;
    VkSemaphoreWaitInfo wait_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
    wait_info.semaphoreCount      = static_cast<u32>(fences.size());
    wait_info.pSemaphores         = semaphores.data();
    wait_info.pValues             = wait_values.data();
    VK_CHECK(vkWaitSemaphores(device, &wait_info, timeout));
}

void Device::wait_idle()
{
    ZoneScoped;
    VK_CHECK(vkDeviceWaitIdle(device));
}

bool Device::acquire_next_swapchain(Surface &surface)
{
    ZoneScoped;
    bool error = false;

    surface.previous_image = surface.current_image;

    auto res = vkAcquireNextImageKHR(device, surface.swapchain, std::numeric_limits<uint64_t>::max(), surface.image_acquired_semaphores[surface.current_image], nullptr, &surface.current_image);

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
} // namespace vulkan
