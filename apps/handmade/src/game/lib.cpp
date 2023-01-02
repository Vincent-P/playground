#include "rhi/context.h"
#include "rhi/device.h"
#include "exo/logger.h"
#include <cstdlib>

struct GameState
{
	int counter;
	rhi::Context rhi_context;
	rhi::Device rhi_device;
};

extern "C" __declspec(dllexport) GameState *init()
{
	auto *state = static_cast<GameState *>(calloc(1, sizeof(GameState)));

	state->rhi_context = rhi::Context::create({.enable_validation = true});


	auto &physical_devices = state->rhi_context.physical_devices;
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
	state->rhi_device = rhi::Device::create(state->rhi_context, device_desc);

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
	state->rhi_device.wait_idle();
	state->rhi_device.destroy(state->rhi_context);
	state->rhi_context.destroy();

	free(state);
}
