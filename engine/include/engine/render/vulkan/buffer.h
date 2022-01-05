#pragma once
#include "engine/render/vulkan/queues.h"
#include "engine/render/vulkan/operators.h"

#include <volk.h>
#include <vk_mem_alloc.h>
#include <string>

namespace vulkan
{

inline constexpr VkBufferUsageFlags storage_buffer_usage  = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
inline constexpr VkBufferUsageFlags index_buffer_usage    = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
inline constexpr VkBufferUsageFlags uniform_buffer_usage  = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
inline constexpr VkBufferUsageFlags source_buffer_usage   = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
inline constexpr VkBufferUsageFlags indirext_buffer_usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

enum struct BufferUsage
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
    std::string        name         = "No name";
    usize              size         = 1;
    VkBufferUsageFlags usage        = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    VmaMemoryUsage     memory_usage = VMA_MEMORY_USAGE_GPU_ONLY;

    bool operator==(const BufferDescription &b) const = default;
};

struct Buffer
{
    BufferDescription desc;
    VkBuffer          vkhandle;
    VmaAllocation     allocation;
    BufferUsage       usage = BufferUsage::None;
    void *            mapped;
    u64               gpu_address;
    u32               descriptor_idx = u32_invalid;

    bool operator==(const Buffer &b) const = default;
};

} // namespace vulkan
