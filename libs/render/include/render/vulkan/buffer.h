#pragma once
#include "exo/maths/numerics.h"

#include "render/vulkan/memory.h"

#include "exo/string.h"
#include <volk.h>

namespace vulkan
{

inline constexpr VkBufferUsageFlags storage_buffer_usage =
	VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
inline constexpr VkBufferUsageFlags index_buffer_usage =
	VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
inline constexpr VkBufferUsageFlags uniform_buffer_usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
inline constexpr VkBufferUsageFlags source_buffer_usage =
	VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
inline constexpr VkBufferUsageFlags indirect_buffer_usage =
	VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

enum struct BufferUsage : u8
{
	None,
	GraphicsShaderRead,
	GraphicsShaderReadWrite,
	ComputeShaderRead,
	ComputeShaderReadWrite,
	TransferDst,
	TransferSrc,
	IndexBuffer,
	VertexBuffer,
	DrawCommands,
	HostWrite
};

struct BufferAccess
{
	VkPipelineStageFlags stage  = 0;
	VkAccessFlags        access = 0;
	// queue?
};

struct BufferDescription
{
	exo::String        name         = "No name";
	usize              size         = 1;
	VkBufferUsageFlags usage        = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	MemoryUsage        memory_usage = MemoryUsage::PREFER_DEVICE;

	bool operator==(const BufferDescription &b) const = default;
};

struct Buffer
{
	BufferDescription desc;
	VkBuffer          vkhandle;
	Allocation        allocation;
	BufferUsage       usage = BufferUsage::None;
	void             *mapped;
	u64               gpu_address;
	u32               descriptor_idx = u32_invalid;

	bool operator==(const Buffer &b) const = default;
};

} // namespace vulkan
