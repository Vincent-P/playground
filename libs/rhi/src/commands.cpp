#include "rhi/commands.h"

#include "exo/collections/dynamic_array.h"
#include "exo/collections/enum_array.h"
#include "exo/profile.h"
#include "rhi/context.h"
#include "rhi/image.h"
#include "rhi/queues.h"
#include "rhi/surface.h"
#include <vulkan/vulkan_core.h>

namespace rhi
{

/// --- Work

void Work::begin()
{
	EXO_PROFILE_SCOPE;
	VkCommandBufferBeginInfo begin_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	begin_info.flags = 0;
	ctx->vkdevice.BeginCommandBuffer(this->command_buffer, &begin_info);
}

void Work::end()
{
	EXO_PROFILE_SCOPE;
	ctx->vkdevice.EndCommandBuffer(this->command_buffer);
}

void Work::wait_for_acquired(Surface *surface, VkPipelineStageFlags stage_dst)
{
	this->image_acquired_semaphore = surface->image_acquired_semaphores[surface->previous_image];
	this->image_acquired_stage = stage_dst;
}

void Work::prepare_present(Surface *surface)
{
	this->signal_present_semaphore = surface->can_present_semaphores[surface->current_image];
}

void Work::begin_debug_label(exo::StringView label, float4 color)
{
	VkDebugUtilsLabelEXT label_info = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
	label_info.pLabelName = label.data();
	label_info.color[0] = color[0];
	label_info.color[1] = color[1];
	label_info.color[2] = color[2];
	label_info.color[3] = color[3];

	ctx->vkdevice.CmdBeginDebugUtilsLabelEXT(this->command_buffer, &label_info);
}

void Work::end_debug_label()
{
	ctx->vkdevice.CmdEndDebugUtilsLabelEXT(this->command_buffer);
}

inline constexpr exo::EnumArray<VkImageLayout, ImageUsage> USAGE_TO_VK_LAYOUT = {
	VK_IMAGE_LAYOUT_UNDEFINED,
	VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	VK_IMAGE_LAYOUT_GENERAL,
	VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	VK_IMAGE_LAYOUT_GENERAL,
	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
	VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
	VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
	VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
};

inline constexpr exo::EnumArray<VkAccessFlags2, ImageUsage> USAGE_TO_SRC_ACCESS = {
	VK_ACCESS_2_NONE,
	VK_ACCESS_2_SHADER_READ_BIT,
	VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR,
	VK_ACCESS_2_SHADER_READ_BIT,
	VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR,
	VK_ACCESS_2_TRANSFER_WRITE_BIT,
	VK_ACCESS_2_TRANSFER_READ_BIT,
	VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
	VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
	VK_ACCESS_2_NONE,
};

inline constexpr exo::EnumArray<VkPipelineStageFlags2, ImageUsage> USAGE_TO_SRC_PIPELINE_STAGE = {
	VK_PIPELINE_STAGE_2_NONE,
	VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
	VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
	VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
	VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
	VK_PIPELINE_STAGE_2_TRANSFER_BIT,
	VK_PIPELINE_STAGE_2_TRANSFER_BIT,
	VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
	VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
	VK_PIPELINE_STAGE_2_NONE,
};

inline constexpr exo::EnumArray<VkAccessFlags2, ImageUsage> USAGE_TO_DST_ACCESS = {
	VK_ACCESS_2_NONE,
	VK_ACCESS_2_SHADER_READ_BIT,
	VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR,
	VK_ACCESS_2_SHADER_READ_BIT,
	VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR,
	VK_ACCESS_2_TRANSFER_WRITE_BIT,
	VK_ACCESS_2_TRANSFER_READ_BIT,
	VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
	VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
	VK_ACCESS_2_NONE,
};

inline constexpr exo::EnumArray<VkPipelineStageFlags2, ImageUsage> USAGE_TO_DST_PIPELINE_STAGE = {
	VK_PIPELINE_STAGE_2_NONE,
	VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
	VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
	VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
	VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
	VK_PIPELINE_STAGE_2_TRANSFER_BIT,
	VK_PIPELINE_STAGE_2_TRANSFER_BIT,
	VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
	VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
	VK_PIPELINE_STAGE_2_NONE,
};

void Work::barrier(Handle<Image> image_handle, ImageUsage new_usage)
{
	auto &image = this->ctx->images.get(image_handle);
	if (image.usage == new_usage) {
		return;
	}

	VkImageMemoryBarrier2 image_barrier = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
	image_barrier.srcStageMask = USAGE_TO_SRC_PIPELINE_STAGE[image.usage];
	image_barrier.dstStageMask = USAGE_TO_DST_PIPELINE_STAGE[new_usage];
	image_barrier.srcAccessMask = USAGE_TO_SRC_ACCESS[image.usage];
	image_barrier.dstAccessMask = USAGE_TO_SRC_ACCESS[new_usage];
	image_barrier.oldLayout = USAGE_TO_VK_LAYOUT[image.usage];
	image_barrier.newLayout = USAGE_TO_VK_LAYOUT[new_usage];
	image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	image_barrier.image = image.vkhandle;
	image_barrier.subresourceRange = image.full_view.range;

	VkDependencyInfo dependency_info = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
	dependency_info.imageMemoryBarrierCount = 1;
	dependency_info.pImageMemoryBarriers = &image_barrier;

	this->ctx->vkdevice.CmdPipelineBarrier2(this->command_buffer, &dependency_info);

	image.usage = new_usage;
}

void Work::clear_image(Handle<Image> image_handle, VkClearColorValue clear_color)
{
	EXO_PROFILE_SCOPE;
	auto &image = this->ctx->images.get(image_handle);

	this->ctx->vkdevice.CmdClearColorImage(command_buffer,
		image.vkhandle,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		&clear_color,
		1,
		&image.full_view.range);
}

// -- Submission

Work Context::get_work()
{
	auto i_frame = this->frame_count % FRAME_BUFFERING;

	u32 i_work = 0;
	for (auto is_used : this->command_buffers_is_used[i_frame]) {
		if (!is_used)
			break;
		i_work += 1;
	}

	Work work = {};
	work.ctx = this;
	if (i_work == this->command_buffers[i_frame].len()) {
		VkCommandBufferAllocateInfo cmdbuffer_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
		cmdbuffer_info.commandPool = this->command_pools[i_frame];
		cmdbuffer_info.commandBufferCount = 1;
		this->vkdevice.AllocateCommandBuffers(this->device, &cmdbuffer_info, &work.command_buffer);
	}
	return work;
}

void Context::submit(Work *work)
{
	EXO_PROFILE_SCOPE;

	exo::DynamicArray<VkSemaphoreSubmitInfo, MAX_SEMAPHORES> wait_semaphore_infos = {};
	exo::DynamicArray<VkSemaphoreSubmitInfo, MAX_SEMAPHORES> signal_semaphore_infos = {};

	// If we requested to signal the "present" semaphore of a Surface
	if (work->signal_present_semaphore != VK_NULL_HANDLE) {
		signal_semaphore_infos.push(VkSemaphoreSubmitInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = work->signal_present_semaphore,
			.value = 0,
		});
	}

	// If we requested to wait for a "image acquired" semaphore of a Surface
	if (work->image_acquired_semaphore != VK_NULL_HANDLE) {
		wait_semaphore_infos.push(VkSemaphoreSubmitInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = work->image_acquired_semaphore,
			.value = 0,
			.stageMask = work->image_acquired_stage,
		});
	}

	VkCommandBufferSubmitInfo cmdbuffer_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
	cmdbuffer_info.commandBuffer = work->command_buffer;

	VkSubmitInfo2 submit_info = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
	submit_info.waitSemaphoreInfoCount = wait_semaphore_infos.len();
	submit_info.pWaitSemaphoreInfos = wait_semaphore_infos.data();
	submit_info.commandBufferInfoCount = 1;
	submit_info.pCommandBufferInfos = &cmdbuffer_info;
	submit_info.signalSemaphoreInfoCount = signal_semaphore_infos.len();
	submit_info.pSignalSemaphoreInfos = signal_semaphore_infos.data();

	VkQueue queue;
	this->vkdevice.GetDeviceQueue(this->device, this->graphics_family_idx, 0, &queue);

	this->vkdevice.QueueSubmit2(queue, 1, &submit_info, VK_NULL_HANDLE);
}

bool Context::present(Surface *surface)
{
	EXO_PROFILE_SCOPE;
	VkPresentInfoKHR present_info = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &surface->can_present_semaphores[surface->current_image];
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &surface->swapchain;
	present_info.pImageIndices = &surface->current_image;

	VkQueue queue;
	this->vkdevice.GetDeviceQueue(this->device, this->graphics_family_idx, 0, &queue);
	auto res = this->vkdevice.QueuePresentKHR(queue, &present_info);

	if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
		return true;
	}

	return false;
}

void Context::wait_idle()
{
	EXO_PROFILE_SCOPE;
	this->vkdevice.DeviceWaitIdle(this->device);
}

bool Context::acquire_next_backbuffer(Surface *surface)
{
	EXO_PROFILE_SCOPE;
	bool error = false;

	surface->previous_image = surface->current_image;

	auto res = this->vkdevice.AcquireNextImageKHR(this->device,
		surface->swapchain,
		u64(~0llu),
		surface->image_acquired_semaphores[surface->current_image],
		nullptr,
		&surface->current_image);

	if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
		error = true;
	}

	return error;
}
} // namespace rhi
