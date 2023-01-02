#include "exo/logger.h"
#include "rhi/context.h"
#include "rhi/device.h"
#include "rhi/surface.h"
#include "rhi/synchronization.h"
#include <cstdlib>

// -- Platform

struct PlatformWindow
{
	uint64_t display_handle;
	uint64_t window_handle;
};

// -- Game

struct RenderState
{
	rhi::Context context;
	rhi::Device device;
	rhi::Surface surface;
	rhi::Fence fence;
};

struct GameState
{
	int counter;
	RenderState render;
};

void init_renderstate(RenderState *render_state, PlatformWindow *window)
{
	render_state->context = rhi::Context::create({.enable_validation = true});

	auto &physical_devices = render_state->context.physical_devices;
	u32 i_selected = u32_invalid;
	u32 i_device = 0;
	for (auto &physical_device : physical_devices) {
		exo::logger::info("Found device: %s\n", physical_device.properties.deviceName);
		if (i_device == u32_invalid && physical_device.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			exo::logger::info("Prioritizing device %s because it is a discrete GPU.\n",
				physical_device.properties.deviceName);
			i_selected = i_device;
		}
		i_device += 1;
	}
	if (i_selected == u32_invalid) {
		i_selected = 0;
		exo::logger::info("No discrete GPU found, defaulting to device #0: %s.\n",
			physical_devices[0].properties.deviceName);
	}

	rhi::DeviceDescription device_desc = {};
	device_desc.physical_device = &physical_devices[i_selected];

	// Create the GPU
	render_state->device = rhi::Device::create(render_state->context, device_desc);
	render_state->surface = rhi::Surface::create(render_state->context,
		render_state->device,
		window->display_handle,
		window->window_handle);
	render_state->fence = render_state->device.create_fence();
}

void shutdown_renderstate(RenderState *render_state)
{
	render_state->device.wait_idle();
	render_state->device.destroy_fence(render_state->fence);
	render_state->surface.destroy(render_state->context, render_state->device);
	render_state->device.destroy(render_state->context);
	render_state->context.destroy();
}

extern "C" __declspec(dllexport) GameState *init(PlatformWindow *platform_window)
{
	auto *state = static_cast<GameState *>(calloc(1, sizeof(GameState)));
	init_renderstate(&state->render, platform_window);
	return state;
}

extern "C" __declspec(dllexport) void reload(GameState *state)
{
	// do nothing
	(void)(state);
}

extern "C" __declspec(dllexport) int update(GameState *state)
{
	state->counter += 1;
	state->counter += 3;
	state->counter -= 3;
	state->counter -= 2;
	state->counter += 2;
	return state->counter;
}

extern "C" __declspec(dllexport) void shutdown(GameState *state)
{
	shutdown_renderstate(&state->render);
	free(state);
}
