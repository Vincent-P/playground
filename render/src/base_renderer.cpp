#include "render/base_renderer.h"
#include "render/vulkan/descriptor_set.h"
#include "render/vulkan/queues.h"

#include <cross/window.h>
#include <exo/logger.h>
#include <exo/memory/scope_stack.h>
#include <exo/memory/string_repository.h>

#include <Tracy.hpp>

BaseRenderer *BaseRenderer::create(exo::ScopeStack &scope, cross::Window *window, gfx::DeviceDescription desc)
{
	BaseRenderer *renderer = scope.allocate<BaseRenderer>();

	renderer->str_repo = exo::StringRepository::create();

	// Initialize the API
	renderer->window  = window;
	renderer->context = gfx::Context::create({.enable_validation = true});

	// Pick a GPU
	auto &physical_devices = renderer->context.physical_devices;
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
	desc.physical_device = &physical_devices[i_selected];

	// Create the GPU
	renderer->device = gfx::Device::create(renderer->context, desc);
	auto &device     = renderer->device;

	// Create an empty image to full the slot #0 on bindless descriptors
	renderer->empty_image =
		device.create_image({.name = "Empty image", .usages = gfx::sampled_image_usage | gfx::storage_image_usage});

	// Create the drawing surface
	renderer->surface = gfx::Surface::create(renderer->context, device, *window);

	for (auto &work_pool : renderer->work_pools) {
		device.create_work_pool(work_pool);
	}

	for (auto &timing : renderer->timings) {
		timing = RenderTimings::create(device, &renderer->str_repo);
	}

	// Prepare the frame synchronization
	renderer->fence = device.create_fence();

	renderer->dynamic_uniform_buffer = RingBuffer::create(device,
	                                                      {
															  .name               = "Dynamic Uniform",
															  .size               = 128_KiB,
															  .gpu_usage          = gfx::uniform_buffer_usage,
															  .frame_queue_length = FRAME_QUEUE_LENGTH,
														  });

	renderer->dynamic_vertex_buffer = RingBuffer::create(device,
	                                                     {
															 .name               = "Dynamic vertices",
															 .size               = 8_MiB,
															 .gpu_usage          = gfx::storage_buffer_usage,
															 .frame_queue_length = FRAME_QUEUE_LENGTH,
														 });

	renderer->dynamic_index_buffer = RingBuffer::create(device,
	                                                    {
															.name               = "Dynamic indices",
															.size               = 8_MiB,
															.gpu_usage          = gfx::index_buffer_usage,
															.frame_queue_length = FRAME_QUEUE_LENGTH,
														});

	renderer->streamer = Streamer::create(&renderer->device, FRAME_QUEUE_LENGTH);

	return renderer;
}

BaseRenderer::~BaseRenderer()
{
	streamer.destroy();

	device.wait_idle();

	device.destroy_fence(fence);

	for (auto &work_pool : work_pools) {
		device.destroy_work_pool(work_pool);
	}

	for (auto &timing : timings) {
		timing.destroy(device);
	}
	surface.destroy(context, device);
	device.destroy(context);
	context.destroy();
}

static gfx::DynamicBufferDescriptor &find_or_create_uniform_descriptor(BaseRenderer &renderer, usize range_size)
{
	for (auto &dynamic_descriptor : renderer.dynamic_descriptors) {
		if (dynamic_descriptor.size == range_size) {
			return dynamic_descriptor;
		}
	}

	renderer.dynamic_descriptors.push_back(
		gfx::create_buffer_descriptor(renderer.device, renderer.dynamic_uniform_buffer.buffer, range_size));
	return renderer.dynamic_descriptors.back();
}

void *BaseRenderer::bind_compute_shader_options(gfx::ComputeWork &cmd, usize options_len)
{
	auto [options, options_offset] = dynamic_uniform_buffer.allocate(device, options_len);
	auto &descriptor               = find_or_create_uniform_descriptor(*this, options_len);
	cmd.bind_uniform_set(descriptor, options_offset, gfx::QueueType::Compute);
	return options;
}

void *BaseRenderer::bind_graphics_shader_options(gfx::GraphicsWork &cmd, usize options_len)
{
	auto [options, options_offset] = dynamic_uniform_buffer.allocate(device, options_len);
	auto &descriptor               = find_or_create_uniform_descriptor(*this, options_len);
	cmd.bind_uniform_set(descriptor, options_offset, gfx::QueueType::Graphics);
	return options;
}

void *BaseRenderer::bind_global_options(gfx::GraphicsWork &cmd, usize options_len)
{
	auto [options, options_offset] = dynamic_uniform_buffer.allocate(device, options_len);
	auto &descriptor               = find_or_create_uniform_descriptor(*this, options_len);
	cmd.bind_uniform_set(descriptor, options_offset, gfx::QueueType::Compute, 1);
	cmd.bind_uniform_set(descriptor, options_offset, gfx::QueueType::Graphics, 1);
	return options;
}

void BaseRenderer::reload_shader(std::string_view shader_name)
{
	device.wait_idle();

	exo::logger::info("{} changed!\n", shader_name);

	// Find the shader that needs to be updated
	gfx::Shader *found = nullptr;
	for (auto [shader_h, shader] : device.shaders) {
		if (shader_name == shader->filename) {
			ASSERT(found == nullptr);
			found = &(*shader);
		}
	}

	if (!found) {
		ASSERT(false);
		return;
	}

	gfx::Shader &shader = *found;

	Vec<Handle<gfx::Shader>> to_remove;

	// Update programs using this shader to the new shader
	for (auto [program_h, program] : device.compute_programs) {
		if (program->state.shader.is_valid()) {
			auto *compute_shader = device.shaders.get(program->state.shader);
			if (!compute_shader) {
				to_remove.push_back(program->state.shader);
			} else if (compute_shader->filename == shader.filename) {
				Handle<gfx::Shader> new_shader = device.create_shader(shader_name);
				exo::logger::info("Found a program using the shader, creating the new shader module #{}\n",
				                  new_shader.value());

				to_remove.push_back(program->state.shader);
				program->state.shader = new_shader;
				device.recreate_program_internal(*program);
			}
		}
	}

	// Destroy the old shaders
	for (Handle<gfx::Shader> shader_h : to_remove) {
		exo::logger::info("Removing old shader #{}\n", shader_h.value());
		device.destroy_shader(shader_h);
	}
	exo::logger::info("\n");
}

void BaseRenderer::on_resize()
{
	device.wait_idle();
	surface.recreate_swapchain(device);
}

bool BaseRenderer::start_frame()
{
	ZoneScoped;

	auto current_frame = frame_count % FRAME_QUEUE_LENGTH;

	// wait for fence, blocking: dont wait for the first QUEUE_LENGTH frames
	u64 wait_value = frame_count < FRAME_QUEUE_LENGTH ? 0 : frame_count - FRAME_QUEUE_LENGTH + 1;
	device.wait_for_fences(std::array{fence}, std::array{wait_value});

	// reset the command buffers
	auto &work_pool = work_pools[current_frame];
	auto &timing    = timings[current_frame];

	device.reset_work_pool(work_pool);

	timing.get_results(device);
	// use timings
	timing.reset(device);

	dynamic_uniform_buffer.start_frame();
	dynamic_vertex_buffer.start_frame();
	dynamic_index_buffer.start_frame();

	bool out_of_date_swapchain = device.acquire_next_swapchain(surface);
	return out_of_date_swapchain;
}

bool BaseRenderer::end_frame(gfx::ComputeWork &cmd)
{
	ZoneScoped;

	// vulkan hack: hint the device to submit a semaphore to wait on before presenting
	cmd.prepare_present(surface);

	device.submit(cmd, std::array{fence}, std::array{frame_count + 1_uz});

	// present will wait for semaphore
	bool out_of_date_swapchain = device.present(surface, cmd);
	frame_count += 1;
	if (out_of_date_swapchain) {
		return true;
	}

	return false;
}
