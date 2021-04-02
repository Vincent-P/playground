#pragma once
#include "base/types.hpp"
#include "base/vector.hpp"
#include "base/logger.hpp"

#include "render/vulkan/resources.hpp"
#include "vulkan/vulkan_core.h"

#include <stdexcept>

namespace vulkan
{
inline const char *vkres_to_str(VkResult code)
{
    switch (code)
    {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_EVENT_SET: return "VK_EVENT_SET";
    case VK_EVENT_RESET: return "VK_EVENT_RESET";
    case VK_INCOMPLETE: return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
    case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
    case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
    case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
    case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
    // case VK_ERROR_INCOMPATIBLE_VERSION_KHR: return "VK_ERROR_INCOMPATIBLE_VERSION_KHR";
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
    case VK_ERROR_NOT_PERMITTED_EXT: return "VK_ERROR_NOT_PERMITTED_EXT";
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
    case VK_THREAD_IDLE_KHR: return "VK_THREAD_IDLE_KHR";
    case VK_THREAD_DONE_KHR: return "VK_THREAD_DONE_KHR";
    case VK_OPERATION_DEFERRED_KHR: return "VK_OPERATION_DEFERRED_KHR";
    case VK_OPERATION_NOT_DEFERRED_KHR: return "VK_OPERATION_NOT_DEFERRED_KHR";
    case VK_PIPELINE_COMPILE_REQUIRED_EXT: return "VK_PIPELINE_COMPILE_REQUIRED_EXT";
    default: break;
    }
    return "Unkown VkResult";
}

#define VK_CHECK(x)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        VkResult err = x;                                                                                              \
        if (err)                                                                                                       \
        {                                                                                                              \
            const char *err_msg = vkres_to_str(err);                                                                   \
            logger::error("Vulkan function returned {}\n", err_msg);    \
            throw std::runtime_error(err_msg);                                                                         \
        }                                                                                                              \
    } while (0)

inline bool is_extension_installed(const char *wanted, const Vec<VkExtensionProperties> &installed)
{
    for (const auto &extension : installed)
    {
        if (!strcmp(wanted, extension.extensionName))
        {
            return true;
        }
    }
    return false;
}

inline ImageAccess get_src_image_access(ImageUsage usage)
{
    ImageAccess access;
    switch (usage)
    {
    case ImageUsage::GraphicsShaderRead:
    {
        access.stage  = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        access.access = 0;
        access.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    break;
    case ImageUsage::GraphicsShaderReadWrite:
    {
        access.stage  = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        access.access = VK_ACCESS_SHADER_WRITE_BIT;
        access.layout = VK_IMAGE_LAYOUT_GENERAL;
    }
    break;
    case ImageUsage::ComputeShaderRead:
    {
        access.stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        access.access = 0;
        access.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    break;
    case ImageUsage::ComputeShaderReadWrite:
    {
        access.stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        access.access = VK_ACCESS_SHADER_WRITE_BIT;
        access.layout = VK_IMAGE_LAYOUT_GENERAL;
    }
    break;
    case ImageUsage::TransferDst:
    {
        access.stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
        access.access = VK_ACCESS_TRANSFER_WRITE_BIT;
        access.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    }
    break;
    case ImageUsage::TransferSrc:
    {
        access.stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
        access.access = 0;
        access.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }
    break;
    case ImageUsage::ColorAttachment:
    {
        access.stage  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        access.access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        access.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    break;
    case ImageUsage::DepthAttachment:
    {
        access.stage  = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        access.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        access.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
    break;
    case ImageUsage::Present:
    {
        access.stage  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        access.access = 0;
        access.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }
    break;
    case ImageUsage::None:
    {
        access.stage  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        access.access = 0;
        access.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    }
    break;
    default:
        assert(false);
        break;
    };
    return access;
}

inline ImageAccess get_dst_image_access(ImageUsage usage)
{
    ImageAccess access;
    switch (usage)
    {
    case ImageUsage::GraphicsShaderRead:
    {
        access.stage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        access.access = VK_ACCESS_SHADER_READ_BIT;
        access.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    break;
    case ImageUsage::GraphicsShaderReadWrite:
    {
        access.stage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        access.access = VK_ACCESS_SHADER_WRITE_BIT;
        access.layout = VK_IMAGE_LAYOUT_GENERAL;
    }
    break;
    case ImageUsage::ComputeShaderRead:
    {
        access.stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        access.access = VK_ACCESS_SHADER_READ_BIT;
        access.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    break;
    case ImageUsage::ComputeShaderReadWrite:
    {
        access.stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        access.access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        access.layout = VK_IMAGE_LAYOUT_GENERAL;
    }
    break;
    case ImageUsage::TransferDst:
    {
        access.stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
        access.access = VK_ACCESS_TRANSFER_WRITE_BIT;
        access.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    }
    break;
    case ImageUsage::TransferSrc:
    {
        access.stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
        access.access = VK_ACCESS_TRANSFER_READ_BIT;
        access.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }
    break;
    case ImageUsage::ColorAttachment:
    {
        access.stage  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        access.access = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        access.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    break;
    case ImageUsage::DepthAttachment:
    {
        access.stage  = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        access.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        access.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
    break;
    case ImageUsage::Present:
    {
        access.stage  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        access.access = 0;
        access.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }
    break;
    case ImageUsage::None:
    {
        access.stage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        access.access = 0;
        access.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    }
    break;
    default:
        assert(false);
        break;
    };
    return access;
}

inline bool is_image_barrier_needed(ImageUsage src, ImageUsage dst)
{
    return !(src == ImageUsage::GraphicsShaderRead && dst == ImageUsage::GraphicsShaderRead);
}

inline VkImageMemoryBarrier get_image_barrier(VkImage image, const ImageAccess &src, const ImageAccess &dst, const VkImageSubresourceRange &range)
{
    VkImageMemoryBarrier b = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.oldLayout            = src.layout;
    b.newLayout            = dst.layout;
    b.srcAccessMask        = src.access;
    b.dstAccessMask        = dst.access;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image                = image;
    b.subresourceRange     = range;
    return b;
}


inline BufferAccess get_src_buffer_access(BufferUsage usage)
{
    BufferAccess access;
    switch (usage)
    {
    case BufferUsage::GraphicsShaderRead:
    {
        access.stage  = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        access.access = 0;
    }
    break;
    case BufferUsage::GraphicsShaderReadWrite:
    {
        access.stage  = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        access.access = VK_ACCESS_SHADER_WRITE_BIT;
    }
    break;
    case BufferUsage::ComputeShaderRead:
    {
        access.stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        access.access = 0;
    }
    break;
    case BufferUsage::ComputeShaderReadWrite:
    {
        access.stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        access.access = VK_ACCESS_SHADER_WRITE_BIT;
    }
    break;
    case BufferUsage::TransferDst:
    {
        access.stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
        access.access = VK_ACCESS_TRANSFER_WRITE_BIT;
    }
    break;
    case BufferUsage::TransferSrc:
    {
        access.stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
        access.access = 0;
    }
    break;
    case BufferUsage::IndexBuffer:
    {
        access.stage  = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        access.access = VK_ACCESS_INDEX_READ_BIT;
    }
    break;
    case BufferUsage::VertexBuffer:
    {
        access.stage  = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        access.access = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    }
    break;
    case BufferUsage::DrawCommands:
    {
        access.stage  = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        access.access = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    }
    break;
    case BufferUsage::HostWrite:
    {
        access.stage  = VK_PIPELINE_STAGE_HOST_BIT;
        access.access = VK_ACCESS_HOST_WRITE_BIT;
    }
    break;
    case BufferUsage::None:
    {
        access.stage  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        access.access = 0;
    }
    break;
    default:
        assert(false);
        break;
    };
    return access;
}

inline BufferAccess get_dst_buffer_access(BufferUsage usage)
{
    BufferAccess access;
    switch (usage)
    {
    case BufferUsage::GraphicsShaderRead:
    {
        access.stage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        access.access = VK_ACCESS_SHADER_READ_BIT;
    }
    break;
    case BufferUsage::GraphicsShaderReadWrite:
    {
        access.stage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        access.access = VK_ACCESS_SHADER_WRITE_BIT;
    }
    break;
    case BufferUsage::ComputeShaderRead:
    {
        access.stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        access.access = VK_ACCESS_SHADER_READ_BIT;
    }
    break;
    case BufferUsage::ComputeShaderReadWrite:
    {
        access.stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        access.access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    }
    break;
    case BufferUsage::TransferDst:
    {
        access.stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
        access.access = VK_ACCESS_TRANSFER_WRITE_BIT;
    }
    break;
    case BufferUsage::TransferSrc:
    {
        access.stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
        access.access = VK_ACCESS_TRANSFER_READ_BIT;
    }
    break;
    case BufferUsage::IndexBuffer:
    {
        access.stage  = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        access.access = VK_ACCESS_INDEX_READ_BIT;
    }
    break;
    case BufferUsage::VertexBuffer:
    {
        access.stage  = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        access.access = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    }
    break;
    case BufferUsage::DrawCommands:
    {
        access.stage  = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        access.access = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    }
    break;
    case BufferUsage::HostWrite:
    {
        access.stage  = VK_PIPELINE_STAGE_HOST_BIT;
        access.access = VK_ACCESS_HOST_WRITE_BIT;
    }
    break;
    case BufferUsage::None:
    {
        access.stage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        access.access = 0;
    }
    break;
    default:
        assert(false);
        break;
    };
    return access;
}

inline VkBufferMemoryBarrier get_buffer_barrier(VkBuffer buffer, const BufferAccess &src, const BufferAccess &dst, usize offset, usize size)
{
    VkBufferMemoryBarrier b = {.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    b.srcAccessMask         = src.access;
    b.dstAccessMask         = dst.access;
    b.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    b.buffer                = buffer;
    b.offset                = offset;
    b.size                  = size;
    return b;
}

inline VkPrimitiveTopology to_vk(PrimitiveTopology topology)
{
    switch (topology)
    {
    case PrimitiveTopology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case PrimitiveTopology::PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    }
    return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
}


inline VkDescriptorType to_vk(DescriptorType type)
{
    switch(type.type)
    {
    case DescriptorType::SampledImage: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case DescriptorType::StorageImage: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case DescriptorType::StorageBuffer: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case DescriptorType::DynamicBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    }
    assert(false);
    return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}

inline bool is_depth_format(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT;
}

}
