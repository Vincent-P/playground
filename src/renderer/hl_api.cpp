#include "renderer/hl_api.hpp"
#include <iostream>

namespace my_app::vulkan
{
    API API::create(const Window& window)
    {
	API api;
	api.ctx = Context::create(window);
	return api;
    }

    void API::destroy()
    {
	ctx.destroy();
    }

    void API::on_resize(int width, int height)
    {
	// submit all command buffers
	ctx.on_resize(width, height);
    }

    void API::start_frame()
    {
	auto& frame_resource = ctx.frame_resources.get_current();

        auto wait_result = ctx.device->waitForFences(*frame_resource.fence, VK_TRUE, 10lu*1000lu*1000lu*1000lu);
        if (wait_result == vk::Result::eTimeout)
        {
            throw std::runtime_error("Submitted the frame more than 10 second ago.");
        }

        ctx.device->resetFences(frame_resource.fence.get());

        auto result = ctx.device->acquireNextImageKHR(*ctx.swapchain.handle,
                                                  std::numeric_limits<uint64_t>::max(),
                                                  *frame_resource.image_available,
                                                  nullptr,
                                                  &ctx.swapchain.current_image);

        if (result == vk::Result::eErrorOutOfDateKHR)
        {
            std::cerr << "The swapchain is out of date. Recreating it...\n";
	    ctx.destroy_swapchain();
	    ctx.create_swapchain();
            return;
        }


	frame_resource.commandbuffer->begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
    }

    void API::end_frame()
    {
	auto graphics_queue = ctx.device->getQueue(ctx.graphics_family_idx, 0);

	auto& frame_resource = ctx.frame_resources.get_current();
	frame_resource.commandbuffer->end();

	vk::PipelineStageFlags stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	vk::SubmitInfo kernel{};
	kernel.waitSemaphoreCount = 1;
	kernel.pWaitSemaphores = &frame_resource.image_available.get();
	kernel.pWaitDstStageMask = &stage;
	kernel.commandBufferCount = 1;
	kernel.pCommandBuffers = &frame_resource.commandbuffer.get();
	kernel.signalSemaphoreCount = 1;
	kernel.pSignalSemaphores = &frame_resource.rendering_finished.get();
	graphics_queue.submit(kernel, frame_resource.fence.get());

        // Present the frame
        vk::PresentInfoKHR present_i{};
        present_i.waitSemaphoreCount = 1;
        present_i.pWaitSemaphores = &frame_resource.rendering_finished.get();
        present_i.swapchainCount = 1;
        present_i.pSwapchains = &ctx.swapchain.handle.get();
        present_i.pImageIndices = &ctx.swapchain.current_image;
        graphics_queue.presentKHR(present_i);

	ctx.frame_count += 1;
	ctx.frame_resources.current = ctx.frame_count % ctx.frame_resources.data.size();
    }

    void API::wait_idle()
    {
        ctx.device->waitIdle();
    }
}
