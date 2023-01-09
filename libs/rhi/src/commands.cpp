#include "rhi/commands.h"

#include "exo/collections/dynamic_array.h"
#include "exo/profile.h"
#include "rhi/context.h"
#include "rhi/queues.h"
#include "rhi/surface.h"
#include <vulkan/vulkan.h>
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

	// Creathe list of semaphores to wait
	exo::DynamicArray<VkSemaphore, 4> signal_list = {};
	exo::DynamicArray<u64, 4> local_signal_values = {};

	// If we requested to signal the "present" semaphore of a Surface
	if (work->signal_present_semaphore != VK_NULL_HANDLE) {
		signal_list.push(work->signal_present_semaphore);
		local_signal_values.push(0u);
	}

	exo::DynamicArray<VkSemaphore, MAX_SEMAPHORES + 1> semaphore_list = {};
	exo::DynamicArray<u64, MAX_SEMAPHORES + 1> value_list = {};
	exo::DynamicArray<VkPipelineStageFlags, MAX_SEMAPHORES + 1> stage_list = {};

	// If we requested to wait for a "image acquired" semaphore of a Surface
	if (work->image_acquired_semaphore != VK_NULL_HANDLE) {
		semaphore_list.push(work->image_acquired_semaphore);
		value_list.push(0u);
		stage_list.push(work->image_acquired_stage);
	}

	VkTimelineSemaphoreSubmitInfo timeline_info = {.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
	timeline_info.waitSemaphoreValueCount = static_cast<u32>(value_list.len());
	timeline_info.pWaitSemaphoreValues = value_list.data();
	timeline_info.signalSemaphoreValueCount = static_cast<u32>(local_signal_values.len());
	timeline_info.pSignalSemaphoreValues = local_signal_values.data();

	VkSubmitInfo submit_info = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submit_info.pNext = &timeline_info;
	submit_info.waitSemaphoreCount = static_cast<u32>(semaphore_list.len());
	submit_info.pWaitSemaphores = semaphore_list.data();
	submit_info.pWaitDstStageMask = stage_list.data();
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &work->command_buffer;
	submit_info.signalSemaphoreCount = static_cast<u32>(signal_list.len());
	submit_info.pSignalSemaphores = signal_list.data();

	VkQueue queue;
	this->vkdevice.GetDeviceQueue(this->device, this->graphics_family_idx, 0, &queue);
	this->vkdevice.QueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
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
