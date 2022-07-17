#pragma once

#include "render/ring_buffer.h"
#include "render/vulkan/commands.h"
#include "render/vulkan/device.h"

namespace bindings
{

void *bind_shader_options(vulkan::Device &device, RingBuffer &ring_buffer, vulkan::ComputeWork &cmd, usize options_len)
{
	auto [p_options, offset] =
		ring_buffer.allocate(device, options_len, 0x40); // 0x10 enough on AMD, should probably check device features
	const auto &descriptor = device.find_or_create_uniform_descriptor(ring_buffer.buffer, options_len);
	cmd.bind_uniform_set(descriptor, offset, cmd.queue_type);
	return p_options;
}

} // namespace bindings
