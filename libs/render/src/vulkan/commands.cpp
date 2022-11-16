#include "render/vulkan/commands.h"

#include "render/vulkan/buffer.h"
#include "render/vulkan/descriptor_set.h"
#include "render/vulkan/device.h"
#include "render/vulkan/image.h"
#include "render/vulkan/pipelines.h"
#include "render/vulkan/queries.h"
#include "render/vulkan/queues.h"
#include "render/vulkan/surface.h"
#include "render/vulkan/utils.h"

#include "exo/collections/dynamic_array.h"
#include "exo/profile.h"

#include <volk.h>

namespace vulkan
{

/// --- Work

void Work::begin()
{
	EXO_PROFILE_SCOPE;
	VkCommandBufferBeginInfo binfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	binfo.flags                    = 0;
	vkBeginCommandBuffer(command_buffer, &binfo);

	vkCmdBindDescriptorSets(command_buffer,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		device->global_sets.pipeline_layout,
		0,
		1,
		&device->global_sets.bindless.vkset,
		0,
		nullptr);

	if (queue_type == QueueType::Graphics) {
		vkCmdBindDescriptorSets(command_buffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			device->global_sets.pipeline_layout,
			0,
			1,
			&device->global_sets.bindless.vkset,
			0,
			nullptr);
	}
}

void Work::bind_uniform_set(const DynamicBufferDescriptor &dynamic_descriptor, u32 offset, u32 i_set /*= 2*/)
{
	vkCmdBindDescriptorSets(command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		device->global_sets.pipeline_layout,
		i_set,
		1,
		&dynamic_descriptor.vkset,
		1,
		&offset);
	vkCmdBindDescriptorSets(command_buffer,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		device->global_sets.pipeline_layout,
		i_set,
		1,
		&dynamic_descriptor.vkset,
		1,
		&offset);
}

void Work::end()
{
	EXO_PROFILE_SCOPE;
	vk_check(vkEndCommandBuffer(command_buffer));
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
	EXO_PROFILE_SCOPE;
	auto &buffer = device->buffers.get(buffer_handle);

	auto src_access = get_src_buffer_access(buffer.usage);
	auto dst_access = get_dst_buffer_access(usage_destination);
	auto b          = get_buffer_barrier(buffer.vkhandle, src_access, dst_access, 0, buffer.desc.size);
	vkCmdPipelineBarrier(command_buffer, src_access.stage, dst_access.stage, 0, 0, nullptr, 1, &b, 0, nullptr);

	buffer.usage = usage_destination;
}

void Work::absolute_barrier(Handle<Image> image_handle)
{
	EXO_PROFILE_SCOPE;
	auto &image = device->images.get(image_handle);

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

	vkCmdPipelineBarrier(command_buffer,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&b);
}

void Work::barrier(Handle<Image> image_handle, ImageUsage usage_destination)
{
	EXO_PROFILE_SCOPE;
	auto &image = device->images.get(image_handle);

	auto src_access = get_src_image_access(image.usage);
	auto dst_access = get_dst_image_access(usage_destination);
	auto b          = get_image_barrier(image.vkhandle, src_access, dst_access, image.full_view.range);
	vkCmdPipelineBarrier(command_buffer, src_access.stage, dst_access.stage, 0, 0, nullptr, 0, nullptr, 1, &b);

	image.usage = usage_destination;
}

void Work::clear_barrier(Handle<Image> image_handle, ImageUsage usage_destination)
{
	auto &image = device->images.get(image_handle);

	auto src_access = get_src_image_access(ImageUsage::None);
	auto dst_access = get_dst_image_access(usage_destination);
	auto b          = get_image_barrier(image.vkhandle, src_access, dst_access, image.full_view.range);
	vkCmdPipelineBarrier(command_buffer, src_access.stage, dst_access.stage, 0, 0, nullptr, 0, nullptr, 1, &b);

	image.usage = usage_destination;
}

void Work::barriers(exo::Span<std::pair<Handle<Image>, ImageUsage>> images,
	exo::Span<std::pair<Handle<Buffer>, BufferUsage>>               buffers)
{
	EXO_PROFILE_SCOPE;
	exo::DynamicArray<VkImageMemoryBarrier, 8>  image_barriers  = {};
	exo::DynamicArray<VkBufferMemoryBarrier, 8> buffer_barriers = {};

	VkPipelineStageFlags src_stage = 0;
	VkPipelineStageFlags dst_stage = 0;

	for (auto &[image_handle, usage_dst] : images) {
		auto &image      = device->images.get(image_handle);
		auto  src_access = get_src_image_access(image.usage);
		auto  dst_access = get_dst_image_access(usage_dst);
		image_barriers.push_back(get_image_barrier(image.vkhandle, src_access, dst_access, image.full_view.range));
		src_stage |= src_access.stage;
		dst_stage |= dst_access.stage;

		image.usage = usage_dst;
	}

	for (auto &[buffer_handle, usage_dst] : buffers) {
		auto &buffer     = device->buffers.get(buffer_handle);
		auto  src_access = get_src_buffer_access(buffer.usage);
		auto  dst_access = get_dst_buffer_access(usage_dst);
		buffer_barriers.push_back(get_buffer_barrier(buffer.vkhandle, src_access, dst_access, 0, buffer.desc.size));
		src_stage |= src_access.stage;
		dst_stage |= dst_access.stage;

		buffer.usage = usage_dst;
	}

	vkCmdPipelineBarrier(command_buffer,
		src_stage,
		dst_stage,
		0,
		0,
		nullptr,
		static_cast<u32>(buffer_barriers.size()),
		buffer_barriers.data(),
		static_cast<u32>(image_barriers.size()),
		image_barriers.data());
}

// Queries
void Work::reset_query_pool(QueryPool &query_pool, u32 first_query, u32 count)
{
	EXO_PROFILE_SCOPE;
	vkCmdResetQueryPool(command_buffer, query_pool.vkhandle, first_query, count);
}

void Work::begin_query(QueryPool &query_pool, u32 index)
{
	EXO_PROFILE_SCOPE;
	vkCmdBeginQuery(command_buffer, query_pool.vkhandle, index, 0);
}

void Work::end_query(QueryPool &query_pool, u32 index)
{
	EXO_PROFILE_SCOPE;
	vkCmdEndQuery(command_buffer, query_pool.vkhandle, index);
}

void Work::timestamp_query(QueryPool &query_pool, u32 index)
{
	EXO_PROFILE_SCOPE;
	vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pool.vkhandle, index);
}

void Work::begin_debug_label(exo::StringView label, float4 color)
{
	VkDebugUtilsLabelEXT label_info = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
	label_info.pLabelName           = label.data();
	label_info.color[0]             = color[0];
	label_info.color[1]             = color[1];
	label_info.color[2]             = color[2];
	label_info.color[3]             = color[3];

	vkCmdBeginDebugUtilsLabelEXT(command_buffer, &label_info);
}

void Work::end_debug_label() { vkCmdEndDebugUtilsLabelEXT(command_buffer); }

/// --- TransferWork
void TransferWork::copy_buffer(
	Handle<Buffer> src, Handle<Buffer> dst, exo::Span<const std::tuple<usize, usize, usize>> offsets_src_dst_size)
{
	EXO_PROFILE_SCOPE;
	auto &src_buffer = device->buffers.get(src);
	auto &dst_buffer = device->buffers.get(dst);

	exo::DynamicArray<VkBufferCopy, 16> buffer_copies = {};

	for (usize i_copy = 0; i_copy < offsets_src_dst_size.len(); i_copy += 1) {
		buffer_copies.push_back({
			.srcOffset = std::get<0>(offsets_src_dst_size[i_copy]),
			.dstOffset = std::get<1>(offsets_src_dst_size[i_copy]),
			.size      = std::get<2>(offsets_src_dst_size[i_copy]),
		});
	}

	vkCmdCopyBuffer(command_buffer,
		src_buffer.vkhandle,
		dst_buffer.vkhandle,
		static_cast<u32>(buffer_copies.size()),
		buffer_copies.data());
}

void TransferWork::copy_buffer(Handle<Buffer> src, Handle<Buffer> dst)
{
	EXO_PROFILE_SCOPE;
	auto &src_buffer = device->buffers.get(src);
	auto &dst_buffer = device->buffers.get(dst);

	const VkBufferCopy copy = {
		.srcOffset = 0,
		.dstOffset = 0,
		.size      = std::min(src_buffer.desc.size, dst_buffer.desc.size),
	};

	vkCmdCopyBuffer(command_buffer, src_buffer.vkhandle, dst_buffer.vkhandle, 1, &copy);
}

void TransferWork::copy_image(Handle<Image> src, Handle<Image> dst)
{
	EXO_PROFILE_SCOPE;
	auto &src_image = device->images.get(src);
	auto &dst_image = device->images.get(dst);

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

	vkCmdCopyImage(command_buffer,
		src_image.vkhandle,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		dst_image.vkhandle,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&copy);
}

void TransferWork::blit_image(Handle<Image> src, Handle<Image> dst)
{
	EXO_PROFILE_SCOPE;
	auto &src_image = device->images.get(src);
	auto &dst_image = device->images.get(dst);

	VkImageBlit blit                   = {};
	blit.srcSubresource.aspectMask     = src_image.full_view.range.aspectMask;
	blit.srcSubresource.mipLevel       = src_image.full_view.range.baseMipLevel;
	blit.srcSubresource.baseArrayLayer = src_image.full_view.range.baseArrayLayer;
	blit.srcSubresource.layerCount     = src_image.full_view.range.layerCount;
	blit.srcOffsets[0]                 = {.x = 0, .y = 0, .z = 0};
	blit.srcOffsets[1]                 = {.x = static_cast<i32>(src_image.desc.size.x),
						.y                   = static_cast<i32>(src_image.desc.size.y),
						.z                   = static_cast<i32>(src_image.desc.size.z)};
	blit.dstSubresource.aspectMask     = dst_image.full_view.range.aspectMask;
	blit.dstSubresource.mipLevel       = dst_image.full_view.range.baseMipLevel;
	blit.dstSubresource.baseArrayLayer = dst_image.full_view.range.baseArrayLayer;
	blit.dstSubresource.layerCount     = dst_image.full_view.range.layerCount;
	blit.dstOffsets[0]                 = {.x = 0, .y = 0, .z = 0};
	blit.dstOffsets[1]                 = {.x = static_cast<i32>(dst_image.desc.size.x),
						.y                   = static_cast<i32>(dst_image.desc.size.y),
						.z                   = static_cast<i32>(dst_image.desc.size.z)};

	vkCmdBlitImage(command_buffer,
		src_image.vkhandle,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		dst_image.vkhandle,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&blit,
		VK_FILTER_NEAREST);
}

void TransferWork::copy_buffer_to_image(
	Handle<Buffer> src, Handle<Image> dst, exo::Span<VkBufferImageCopy> buffer_copy_regions)
{
	EXO_PROFILE_SCOPE;
	auto &src_buffer = device->buffers.get(src);
	auto &dst_image  = device->images.get(dst);

	const u32 region_count = static_cast<u32>(buffer_copy_regions.len());
	vkCmdCopyBufferToImage(command_buffer,
		src_buffer.vkhandle,
		dst_image.vkhandle,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		region_count,
		buffer_copy_regions.data());
}

void TransferWork::fill_buffer(Handle<Buffer> buffer_handle, u32 data)
{
	EXO_PROFILE_SCOPE;
	auto &buffer = device->buffers.get(buffer_handle);
	vkCmdFillBuffer(command_buffer, buffer.vkhandle, 0, buffer.desc.size, data);
}
/// --- ComputeWork

void ComputeWork::bind_pipeline(Handle<ComputeProgram> program_handle)
{
	EXO_PROFILE_SCOPE;
	auto &program = device->compute_programs.get(program_handle);
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, program.pipeline);
}

void ComputeWork::dispatch(uint3 workgroups)
{
	EXO_PROFILE_SCOPE;
	vkCmdDispatch(command_buffer, workgroups.x, workgroups.y, workgroups.z);
}

void ComputeWork::clear_image(Handle<Image> image_handle, VkClearColorValue clear_color)
{
	EXO_PROFILE_SCOPE;
	auto image = device->images.get(image_handle);

	vkCmdClearColorImage(command_buffer,
		image.vkhandle,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		&clear_color,
		1,
		&image.full_view.range);
}

void ComputeWork::push_constant(const void *data, usize len)
{
	EXO_PROFILE_SCOPE;
	vkCmdPushConstants(command_buffer,
		device->global_sets.pipeline_layout,
		VK_SHADER_STAGE_ALL,
		0,
		static_cast<u32>(len),
		data);
}

/// --- GraphicsWork

void GraphicsWork::draw_indexed(const DrawIndexedOptions &options)
{
	EXO_PROFILE_SCOPE;
	vkCmdDrawIndexed(command_buffer,
		options.vertex_count,
		options.instance_count,
		options.index_offset,
		options.vertex_offset,
		options.instance_offset);
}

void GraphicsWork::draw(const DrawOptions &options)
{
	EXO_PROFILE_SCOPE;
	vkCmdDraw(command_buffer,
		options.vertex_count,
		options.instance_count,
		options.vertex_offset,
		options.instance_offset);
}

void GraphicsWork::draw_indexed_indirect_count(const DrawIndexedIndirectCountOptions &options)
{
	EXO_PROFILE_SCOPE;
	auto &arguments = device->buffers.get(options.arguments_buffer);
	auto &count     = device->buffers.get(options.count_buffer);
	vkCmdDrawIndexedIndirectCount(command_buffer,
		arguments.vkhandle,
		options.arguments_offset,
		count.vkhandle,
		options.count_offset,
		options.max_draw_count,
		options.stride);
}

void GraphicsWork::set_scissor(const VkRect2D &rect)
{
	EXO_PROFILE_SCOPE;
	vkCmdSetScissor(command_buffer, 0, 1, &rect);
}

void GraphicsWork::set_viewport(const VkViewport &viewport)
{
	EXO_PROFILE_SCOPE;
	vkCmdSetViewport(command_buffer, 0, 1, &viewport);
}

void GraphicsWork::begin_pass(Handle<Framebuffer> framebuffer_handle, exo::Span<const LoadOp> load_ops)
{
	EXO_PROFILE_SCOPE;
	auto &framebuffer = device->framebuffers.get(framebuffer_handle);
	auto &renderpass  = device->find_or_create_renderpass(framebuffer, load_ops);

	exo::DynamicArray<VkClearValue, MAX_ATTACHMENTS> clear_colors = {};
	for (auto &load_op : load_ops) {
		clear_colors.push_back(load_op.color);
	}

	VkRenderPassBeginInfo begin_info    = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
	begin_info.renderPass               = renderpass.vkhandle;
	begin_info.framebuffer              = framebuffer.vkhandle;
	begin_info.renderArea.extent.width  = static_cast<u32>(framebuffer.format.size[0]);
	begin_info.renderArea.extent.height = static_cast<u32>(framebuffer.format.size[1]);
	begin_info.clearValueCount          = static_cast<u32>(clear_colors.size());
	begin_info.pClearValues             = clear_colors.data();

	vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void GraphicsWork::end_pass() { vkCmdEndRenderPass(command_buffer); }

void GraphicsWork::bind_pipeline(Handle<GraphicsProgram> program_handle, uint pipeline_index)
{
	EXO_PROFILE_SCOPE;
	auto &program  = device->graphics_programs.get(program_handle);
	auto  pipeline = program.pipelines[pipeline_index];
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

void GraphicsWork::bind_index_buffer(Handle<Buffer> buffer_handle, VkIndexType index_type, usize offset)
{
	EXO_PROFILE_SCOPE;
	auto &buffer = device->buffers.get(buffer_handle);
	vkCmdBindIndexBuffer(command_buffer, buffer.vkhandle, offset, index_type);
}

/// --- Device

// WorkPool
void Device::create_work_pool(WorkPool &work_pool)
{
	EXO_PROFILE_SCOPE;
	VkCommandPoolCreateInfo pool_info = {
		.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags            = 0,
		.queueFamilyIndex = this->graphics_family_idx,
	};

	vk_check(vkCreateCommandPool(this->device, &pool_info, nullptr, &work_pool.graphics().vk_handle));

	pool_info.queueFamilyIndex = this->compute_family_idx;
	vk_check(vkCreateCommandPool(this->device, &pool_info, nullptr, &work_pool.compute().vk_handle));

	pool_info.queueFamilyIndex = this->transfer_family_idx;
	vk_check(vkCreateCommandPool(this->device, &pool_info, nullptr, &work_pool.transfer().vk_handle));
}

void Device::reset_work_pool(WorkPool &work_pool)
{
	EXO_PROFILE_SCOPE;
	// TODO: Validate that all command buffers are recorded?
	for (auto &command_pool : work_pool.command_pools) {
		for (auto &is_used : command_pool.command_buffers_is_used) {
			is_used = false;
		}

		vk_check(vkResetCommandPool(this->device, command_pool.vk_handle, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT));
	}
}

void Device::destroy_work_pool(WorkPool &work_pool)
{
	EXO_PROFILE_SCOPE;
	for (auto &command_pool : work_pool.command_pools) {
		vkDestroyCommandPool(device, command_pool.vk_handle, nullptr);
	}
}

// CommandPool
void Device::create_query_pool(QueryPool &query_pool, u32 query_capacity)
{
	EXO_PROFILE_SCOPE;
	VkQueryPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
	pool_info.queryType             = VK_QUERY_TYPE_TIMESTAMP;
	pool_info.queryCount            = query_capacity;

	vk_check(vkCreateQueryPool(device, &pool_info, nullptr, &query_pool.vkhandle));

	query_pool.capacity = query_capacity;
}

void Device::reset_query_pool(QueryPool &query_pool, u32 first_query, u32 count)
{
	EXO_PROFILE_SCOPE;
	vkResetQueryPool(device, query_pool.vkhandle, first_query, count);
}

void Device::destroy_query_pool(QueryPool &query_pool)
{
	EXO_PROFILE_SCOPE;
	vkDestroyQueryPool(device, query_pool.vkhandle, nullptr);
	query_pool.vkhandle = VK_NULL_HANDLE;
}

void Device::get_query_results(QueryPool &query_pool, u32 first_query, u32 count, Vec<u64> &results)
{
	EXO_PROFILE_SCOPE;
	const usize old_size = results.len();
	results.resize(old_size + count);

	ASSERT(first_query + count < query_pool.capacity);

	vkGetQueryPoolResults(device,
		query_pool.vkhandle,
		first_query,
		count,
		count * sizeof(u64),
		results.data() + old_size,
		sizeof(u64),
		VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
}

// Work
static Work create_work(Device &device, WorkPool &work_pool, QueueType queue_type)
{
	EXO_PROFILE_SCOPE;
	auto &command_pool = work_pool.command_pools[queue_type];

	Work work           = {};
	work.device         = &device;
	work.command_buffer = VK_NULL_HANDLE;

	for (u32 i_command_buffer = 0; i_command_buffer < command_pool.command_buffers.len(); i_command_buffer += 1) {
		if (command_pool.command_buffers_is_used[i_command_buffer] == false) {
			command_pool.command_buffers_is_used[i_command_buffer] = true;
			work.command_buffer                                    = command_pool.command_buffers[i_command_buffer];
			break;
		}
	}

	if (work.command_buffer == VK_NULL_HANDLE) {
		VkCommandBufferAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
		ai.commandPool                 = command_pool.vk_handle;
		ai.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		ai.commandBufferCount          = 1;
		vk_check(vkAllocateCommandBuffers(device.device, &ai, &work.command_buffer));

		command_pool.command_buffers.push(work.command_buffer);
		command_pool.command_buffers_is_used.push(true);
	}

	const u32 queue_family_idx = queue_type == QueueType::Graphics   ? device.graphics_family_idx
	                             : queue_type == QueueType::Compute  ? device.compute_family_idx
	                             : queue_type == QueueType::Transfer ? device.transfer_family_idx
	                                                                 : u32_invalid;

	ASSERT(queue_family_idx != u32_invalid);
	vkGetDeviceQueue(device.device, queue_family_idx, 0, &work.queue);

	work.queue_type = queue_type;

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
	EXO_PROFILE_SCOPE;
	Fence                     fence         = {};
	VkSemaphoreTypeCreateInfo timeline_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
	timeline_info.semaphoreType             = VK_SEMAPHORE_TYPE_TIMELINE;
	timeline_info.initialValue              = initial_value;

	VkSemaphoreCreateInfo semaphore_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	semaphore_info.pNext                 = &timeline_info;

	vk_check(vkCreateSemaphore(device, &semaphore_info, nullptr, &fence.timeline_semaphore));

	return fence;
}

u64 Device::get_fence_value(Fence &fence)
{
	EXO_PROFILE_SCOPE;
	vk_check(vkGetSemaphoreCounterValue(device, fence.timeline_semaphore, &fence.value));
	return fence.value;
}

void Device::set_fence_value(Fence &fence, u64 value)
{
	EXO_PROFILE_SCOPE;
	VkSemaphoreSignalInfo signal_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO};
	signal_info.semaphore             = fence.timeline_semaphore;
	signal_info.value                 = value;

	vk_check(vkSignalSemaphore(device, &signal_info));
}

void Device::destroy_fence(Fence &fence)
{
	EXO_PROFILE_SCOPE;
	vkDestroySemaphore(device, fence.timeline_semaphore, nullptr);
	fence.timeline_semaphore = VK_NULL_HANDLE;
}

// Submission
void Device::submit(Work &work, exo::Span<const Fence> signal_fences, exo::Span<const u64> signal_values)
{
	EXO_PROFILE_SCOPE;
	// Creathe list of semaphores to wait
	exo::DynamicArray<VkSemaphore, 4> signal_list         = {};
	exo::DynamicArray<u64, 4>         local_signal_values = {signal_values};
	for (const auto &fence : signal_fences) {
		signal_list.push_back(fence.timeline_semaphore);
	}

	if (work.signal_present_semaphore) {
		signal_list.push_back(work.signal_present_semaphore.value());
		local_signal_values.push_back(0);
	}

	exo::DynamicArray<VkSemaphore, MAX_SEMAPHORES + 1>          semaphore_list = {};
	exo::DynamicArray<u64, MAX_SEMAPHORES + 1>                  value_list     = {};
	exo::DynamicArray<VkPipelineStageFlags, MAX_SEMAPHORES + 1> stage_list     = {};

	for (usize i = 0; i < work.wait_fence_list.size(); i++) {
		semaphore_list.push_back(work.wait_fence_list[i].timeline_semaphore);
		value_list.push_back(work.wait_value_list[i]);
		stage_list.push_back(work.wait_stage_list[i]);
	}

	// vulkan hacks
	if (work.image_acquired_semaphore) {
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

	vk_check(vkQueueSubmit(work.queue, 1, &submit_info, VK_NULL_HANDLE));
}

bool Device::present(Surface &surface, Work &work)
{
	EXO_PROFILE_SCOPE;
	VkPresentInfoKHR present_i   = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
	present_i.waitSemaphoreCount = 1;
	present_i.pWaitSemaphores    = &surface.can_present_semaphores[surface.current_image];
	present_i.swapchainCount     = 1;
	present_i.pSwapchains        = &surface.swapchain;
	present_i.pImageIndices      = &surface.current_image;

	auto res = vkQueuePresentKHR(work.queue, &present_i);

	if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
		return true;
	} else {
		vk_check(res);
	}

	return false;
}

void Device::wait_for_fence(const Fence &fence, u64 wait_value)
{
	EXO_PROFILE_SCOPE;
	// 1 sec in nanoseconds
	const u64           timeout   = 1llu * 1000llu * 1000llu * 1000llu;
	VkSemaphoreWaitInfo wait_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
	wait_info.semaphoreCount      = 1;
	wait_info.pSemaphores         = &fence.timeline_semaphore;
	wait_info.pValues             = &wait_value;
	vk_check(vkWaitSemaphores(device, &wait_info, timeout));
}

void Device::wait_for_fences(exo::Span<const Fence> fences, exo::Span<const u64> wait_values)
{
	EXO_PROFILE_SCOPE;
	ASSERT(wait_values.len() == fences.len());

	exo::DynamicArray<VkSemaphore, 4> semaphores = {};
	for (usize i_fence = 0; i_fence < fences.len(); i_fence += 1) {
		semaphores.push_back(fences[i_fence].timeline_semaphore);
	}

	// 1 sec in nanoseconds
	const u64           timeout   = 1llu * 1000llu * 1000llu * 1000llu;
	VkSemaphoreWaitInfo wait_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
	wait_info.semaphoreCount      = static_cast<u32>(fences.len());
	wait_info.pSemaphores         = semaphores.data();
	wait_info.pValues             = wait_values.data();
	vk_check(vkWaitSemaphores(device, &wait_info, timeout));
}

void Device::wait_idle()
{
	EXO_PROFILE_SCOPE;
	vk_check(vkDeviceWaitIdle(device));
}

bool Device::acquire_next_swapchain(Surface &surface)
{
	EXO_PROFILE_SCOPE;
	bool error = false;

	surface.previous_image = surface.current_image;

	auto res = vkAcquireNextImageKHR(device,
		surface.swapchain,
		std::numeric_limits<uint64_t>::max(),
		surface.image_acquired_semaphores[surface.current_image],
		nullptr,
		&surface.current_image);

	if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
		error = true;
	} else {
		vk_check(res);
	}

	return error;
}
} // namespace vulkan
