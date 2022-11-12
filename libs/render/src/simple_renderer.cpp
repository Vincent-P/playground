#include "render/simple_renderer.h"

#include "render/vulkan/buffer.h"
#include "render/vulkan/pipelines.h"
#include "render/vulkan/shader.h"

#include <exo/logger.h>
#include <exo/profile.h>

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

	for (auto &workpool : renderer.workpools) {
		device.create_work_pool(workpool);
	}

	renderer.uniform_buffer = RingBuffer::create(device,
		{
			.name               = "Dynamic Uniform",
			.size               = 512_KiB,
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
			.size               = 128_MiB,
			.gpu_usage          = vulkan::source_buffer_usage,
			.frame_queue_length = FRAME_QUEUE_LENGTH,
		});

	// Create the drawing surface
	renderer.swapchain_node.surface = vulkan::Surface::create(renderer.context, device, window_handle);
	renderer.swapchain_node.fence   = device.create_fence();

	return renderer;
}

void SimpleRenderer::destroy()
{
	this->device.wait_idle();
	this->device.destroy_fence(this->swapchain_node.fence);

	for (auto &workpool : this->workpools) {
		device.destroy_work_pool(workpool);
	}

	this->swapchain_node.surface.destroy(this->context, this->device);
	this->device.destroy(this->context);
	this->context.destroy();
}

void SimpleRenderer::start_frame()
{
	EXO_PROFILE_SCOPE;

	EXO_PROFILE_PLOT_VALUE("Uniforms last frame allocated",
		i64(this->uniform_buffer.frame_size_allocated[this->uniform_buffer.i_frame %
													  this->uniform_buffer.frame_size_allocated.size()]));

	this->uniform_buffer.start_frame();
	this->dynamic_vertex_buffer.start_frame();
	this->dynamic_index_buffer.start_frame();
	this->upload_buffer.start_frame();

	EXO_PROFILE_PLOT_VALUE("Device: shaders", i64(this->device.shaders.size));
	EXO_PROFILE_PLOT_VALUE("Device: graphics programs", i64(this->device.graphics_programs.size));
	EXO_PROFILE_PLOT_VALUE("Device: compute programs", i64(this->device.compute_programs.size));
	EXO_PROFILE_PLOT_VALUE("Device: framebuffers", i64(this->device.framebuffers.size));
	EXO_PROFILE_PLOT_VALUE("Device: images", i64(this->device.images.size));
	EXO_PROFILE_PLOT_VALUE("Device: buffers", i64(this->device.buffers.size));
	EXO_PROFILE_PLOT_VALUE("Device: samplers", i64(this->device.samplers.len()));
}

void SimpleRenderer::render(Handle<TextureDesc> output, float dt)
{
	EXO_PROFILE_SCOPE;
	this->time += dt;

	auto i_frame          = this->swapchain_node.i_frame;
	auto swapchain_output = builtins::acquire_next_image(this->render_graph, this->swapchain_node);

	builtins::blit_image(this->render_graph, output, swapchain_output);
	builtins::present(this->render_graph, this->swapchain_node, i_frame + FRAME_QUEUE_LENGTH);

	auto  current_frame = i_frame % FRAME_QUEUE_LENGTH;
	auto &workpool      = this->workpools[current_frame];
	this->device.wait_for_fence(this->swapchain_node.fence, i_frame);
	this->device.reset_work_pool(workpool);

	this->reload_shaders();

	this->device.update_globals();

	auto pass_api = PassApi{
		.context               = this->context,
		.device                = this->device,
		.uniform_buffer        = this->uniform_buffer,
		.dynamic_vertex_buffer = this->dynamic_vertex_buffer,
		.dynamic_index_buffer  = this->dynamic_index_buffer,
		.upload_buffer         = this->upload_buffer,
	};

	this->render_graph.execute(pass_api, workpool);
}

void SimpleRenderer::end_frame()
{
	this->frame_count += 1;
	this->render_graph.end_frame();
}

const vulkan::Surface &SimpleRenderer::surface() { return this->swapchain_node.surface; }

void SimpleRenderer::reload_shaders()
{
	EXO_PROFILE_SCOPE;

	Vec<Handle<vulkan::Shader>> shaders_to_reload;
	this->shader_watcher.update([&](const auto &watch, const auto &event) {
		auto full_path = watch.path + event.name;
		fmt::print("event: {}\n", full_path);
		for (const auto [shader_handle, p_shader] : this->device.shaders) {
			fmt::print("shader: {}\n", p_shader->filename);
			if (p_shader->filename == full_path) {
				shaders_to_reload.push(shader_handle);
			}
		}
		fmt::print("\n");
	});

	if (shaders_to_reload.is_empty()) {
		return;
	}

	this->device.wait_idle();

	for (auto reloaded_shader : shaders_to_reload) {
		this->device.reload_shader(reloaded_shader);

		for (auto [program_handle, p_program] : this->device.graphics_programs) {
			if (p_program->graphics_state.vertex_shader == reloaded_shader ||
				p_program->graphics_state.fragment_shader == reloaded_shader) {
				for (u32 i_pipeline = 0; i_pipeline < p_program->pipelines.size(); ++i_pipeline) {
					this->device.compile_graphics_pipeline(program_handle, i_pipeline);
				}
			}
		}

		for (auto [program_handle, p_program] : this->device.compute_programs) {
			if (p_program->state.shader == reloaded_shader) {
				this->device.recreate_program_internal(*p_program);
			}
		}
	}
}
