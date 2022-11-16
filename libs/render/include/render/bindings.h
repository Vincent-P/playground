#pragma once

#include "exo/collections/span.h"

#include "render/ring_buffer.h"
#include "render/vulkan/commands.h"
#include "render/vulkan/device.h"

namespace bindings
{
inline exo::Span<u8> bind_shader_options(
	vulkan::Device &device, RingBuffer &ring_buffer, vulkan::ComputeWork &cmd, usize options_len)
{
	auto [p_options, offset] =
		ring_buffer.allocate(options_len, 0x40); // 0x10 enough on AMD, should probably check device features
	const auto &descriptor = device.find_or_create_uniform_descriptor(ring_buffer.buffer, options_len);
	cmd.bind_uniform_set(descriptor, u32(offset));
	return p_options;
}

template <typename Option>
inline exo::Span<Option> bind_option_struct(
	vulkan::Device &device, RingBuffer &ring_buffer, vulkan::ComputeWork &cmd, usize options_count = 1)
{
	auto slice = bind_shader_options(device, ring_buffer, cmd, sizeof(Option) * options_count);
	return exo::reinterpret_span<Option>(slice);
}

} // namespace bindings
