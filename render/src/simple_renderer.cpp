#include "render/simple_renderer.h"

#include "render/vulkan/buffer.h"

#include <exo/logger.h>

SimpleRenderer SimpleRenderer::create(u64 window_handle)
{
	SimpleRenderer renderer;
	renderer.context = vulkan::Context::create({.enable_validation = true});

	// Pick a GPU
	auto &physical_devices = renderer.context.physical_devices;
	u32   i_selected       = u32_invalid;
	u32   i_device         = 0;
	for (auto &physical_device : physical_devices) {
		exo::logger::info("Found device: {}\n", physical_device.properties.deviceName);
		if (i_device == u32_invalid && physical_device.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			exo::logger::info("Prioritizing device {} because it is a discrete GPU.\n",
				physical_device.properties.deviceName);
			i_selected = i_device;
		}
		i_device += 1;
	}
	if (i_selected == u32_invalid) {
		i_selected = 0;
		exo::logger::info("No discrete GPU found, defaulting to device #0: {}.\n",
			physical_devices[0].properties.deviceName);
	}

	vulkan::DeviceDescription device_desc = {};
	device_desc.physical_device           = &physical_devices[i_selected];

	// Create the GPU
	renderer.device = vulkan::Device::create(renderer.context, device_desc);
	auto &device    = renderer.device;

	for (usize i = 0; i < FRAME_QUEUE_LENGTH; ++i) {
		device.create_work_pool(renderer.workpools[i]);
	}

	renderer.uniform_buffer = RingBuffer::create(device,
		{
			.name               = "Dynamic Uniform",
			.size               = 128_KiB,
			.gpu_usage          = vulkan::uniform_buffer_usage,
			.frame_queue_length = FRAME_QUEUE_LENGTH,
		});

	renderer.dynamic_vertex_buffer = RingBuffer::create(device,
		{
			.name               = "Dynamic vertices",
			.size               = 8_MiB,
			.gpu_usage          = vulkan::storage_buffer_usage,
			.frame_queue_length = FRAME_QUEUE_LENGTH,
		});

	renderer.dynamic_index_buffer = RingBuffer::create(device,
		{
			.name               = "Dynamic indices",
			.size               = 8_MiB,
			.gpu_usage          = vulkan::index_buffer_usage,
			.frame_queue_length = FRAME_QUEUE_LENGTH,
		});

	renderer.upload_buffer = RingBuffer::create(device,
		{
			.name               = "Upload buffer",
			.size               = 8_MiB,
			.gpu_usage          = vulkan::source_buffer_usage,
			.frame_queue_length = FRAME_QUEUE_LENGTH,
		});

	// Create the render graph
	renderer.render_graph.resources.image_pool       = exo::IndexMap::with_capacity(16);
	renderer.render_graph.resources.framebuffer_pool = exo::IndexMap::with_capacity(16);

	// Create the drawing surface
	renderer.swapchain_node.surface = vulkan::Surface::create(renderer.context, device, window_handle);
	renderer.swapchain_node.fence   = device.create_fence();

	return renderer;
}

void SimpleRenderer::destroy()
{
	this->device.wait_idle();
	this->device.destroy_fence(this->swapchain_node.fence);

	for (usize i = 0; i < FRAME_QUEUE_LENGTH; ++i) {
		device.destroy_work_pool(this->workpools[i]);
	}

	this->swapchain_node.surface.destroy(this->context, this->device);
	this->device.destroy(this->context);
	this->context.destroy();
}

void SimpleRenderer::render(Handle<TextureDesc> output, float dt)
{
	auto i_frame          = this->swapchain_node.i_frame;
	auto swapchain_output = builtins::acquire_next_image(this->render_graph, this->swapchain_node);

	builtins::blit_image(this->render_graph, output, swapchain_output);
	builtins::present(this->render_graph, this->swapchain_node, i_frame + FRAME_QUEUE_LENGTH);

	auto  current_frame = i_frame % FRAME_QUEUE_LENGTH;
	auto &workpool      = this->workpools[current_frame];
	this->device.wait_for_fence(this->swapchain_node.fence, i_frame);
	this->device.reset_work_pool(workpool);

	// TODO: reload shader

	this->device.update_globals();
	this->uniform_buffer.start_frame();
	this->dynamic_vertex_buffer.start_frame();
	this->dynamic_index_buffer.start_frame();
	this->upload_buffer.start_frame();

	auto pass_api = PassApi{
		.context               = this->context,
		.device                = this->device,
		.uniform_buffer        = this->uniform_buffer,
		.dynamic_vertex_buffer = this->dynamic_vertex_buffer,
		.dynamic_index_buffer  = this->dynamic_index_buffer,
		.upload_buffer         = this->upload_buffer,
	};

	this->render_graph.execute(pass_api, workpool);
	this->frame_count += 1;
	this->time += dt;
}

const vulkan::Surface &SimpleRenderer::surface() { return this->swapchain_node.surface; }
