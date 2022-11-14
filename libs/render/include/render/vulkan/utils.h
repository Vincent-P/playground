#pragma once
#include "render/vulkan/buffer.h"
#include "render/vulkan/image.h"

#include "exo/collections/span.h"
#include "exo/macros/assert.h"
#include <cstring>
#include <volk.h>

namespace vulkan
{

void vk_check(VkResult result);

inline bool is_extension_installed(const char *wanted, exo::Span<const VkExtensionProperties> installed)
{
	for (const auto &extension : installed) {
		if (!strcmp(wanted, extension.extensionName)) {
			return true;
		}
	}
	return false;
}

inline ImageAccess get_src_image_access(ImageUsage usage)
{
	ImageAccess access = {};
	switch (usage) {
	case ImageUsage::GraphicsShaderRead: {
		access.stage  = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
		access.access = 0;
		access.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	} break;
	case ImageUsage::GraphicsShaderReadWrite: {
		access.stage  = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		access.access = VK_ACCESS_SHADER_WRITE_BIT;
		access.layout = VK_IMAGE_LAYOUT_GENERAL;
	} break;
	case ImageUsage::ComputeShaderRead: {
		access.stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		access.access = 0;
		access.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	} break;
	case ImageUsage::ComputeShaderReadWrite: {
		access.stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		access.access = VK_ACCESS_SHADER_WRITE_BIT;
		access.layout = VK_IMAGE_LAYOUT_GENERAL;
	} break;
	case ImageUsage::TransferDst: {
		access.stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
		access.access = VK_ACCESS_TRANSFER_WRITE_BIT;
		access.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	} break;
	case ImageUsage::TransferSrc: {
		access.stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
		access.access = 0;
		access.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	} break;
	case ImageUsage::ColorAttachment: {
		access.stage  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		access.access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		access.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	} break;
	case ImageUsage::DepthAttachment: {
		access.stage  = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		access.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		access.layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	} break;
	case ImageUsage::Present: {
		access.stage  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		access.access = 0;
		access.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	} break;
	case ImageUsage::None: {
		access.stage  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		access.access = 0;
		access.layout = VK_IMAGE_LAYOUT_UNDEFINED;
	} break;
	default:
		ASSERT(false);
		break;
	};
	return access;
}

inline ImageAccess get_dst_image_access(ImageUsage usage)
{
	ImageAccess access;
	switch (usage) {
	case ImageUsage::GraphicsShaderRead: {
		access.stage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		access.access = VK_ACCESS_SHADER_READ_BIT;
		access.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	} break;
	case ImageUsage::GraphicsShaderReadWrite: {
		access.stage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		access.access = VK_ACCESS_SHADER_WRITE_BIT;
		access.layout = VK_IMAGE_LAYOUT_GENERAL;
	} break;
	case ImageUsage::ComputeShaderRead: {
		access.stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		access.access = VK_ACCESS_SHADER_READ_BIT;
		access.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	} break;
	case ImageUsage::ComputeShaderReadWrite: {
		access.stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		access.access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		access.layout = VK_IMAGE_LAYOUT_GENERAL;
	} break;
	case ImageUsage::TransferDst: {
		access.stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
		access.access = VK_ACCESS_TRANSFER_WRITE_BIT;
		access.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	} break;
	case ImageUsage::TransferSrc: {
		access.stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
		access.access = VK_ACCESS_TRANSFER_READ_BIT;
		access.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	} break;
	case ImageUsage::ColorAttachment: {
		access.stage  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		access.access = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		access.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	} break;
	case ImageUsage::DepthAttachment: {
		access.stage  = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		access.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		access.layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	} break;
	case ImageUsage::Present: {
		access.stage  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		access.access = 0;
		access.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	} break;
	case ImageUsage::None: {
		access.stage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		access.access = 0;
		access.layout = VK_IMAGE_LAYOUT_UNDEFINED;
	} break;
	default:
		ASSERT(false);
		break;
	};
	return access;
}

inline bool is_image_barrier_needed(ImageUsage src, ImageUsage dst)
{
	return !(src == ImageUsage::GraphicsShaderRead && dst == ImageUsage::GraphicsShaderRead);
}

inline VkImageMemoryBarrier get_image_barrier(
	VkImage image, const ImageAccess &src, const ImageAccess &dst, const VkImageSubresourceRange &range)
{
	VkImageMemoryBarrier b = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	b.oldLayout            = src.layout;
	b.newLayout            = dst.layout;
	b.srcAccessMask        = src.access;
	b.dstAccessMask        = dst.access;
	b.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
	b.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
	b.image                = image;
	b.subresourceRange     = range;
	return b;
}

inline BufferAccess get_src_buffer_access(BufferUsage usage)
{
	BufferAccess access;
	switch (usage) {
	case BufferUsage::GraphicsShaderRead: {
		access.stage  = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
		access.access = 0;
	} break;
	case BufferUsage::GraphicsShaderReadWrite: {
		access.stage  = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		access.access = VK_ACCESS_SHADER_WRITE_BIT;
	} break;
	case BufferUsage::ComputeShaderRead: {
		access.stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		access.access = 0;
	} break;
	case BufferUsage::ComputeShaderReadWrite: {
		access.stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		access.access = VK_ACCESS_SHADER_WRITE_BIT;
	} break;
	case BufferUsage::TransferDst: {
		access.stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
		access.access = VK_ACCESS_TRANSFER_WRITE_BIT;
	} break;
	case BufferUsage::TransferSrc: {
		access.stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
		access.access = 0;
	} break;
	case BufferUsage::IndexBuffer: {
		access.stage  = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
		access.access = VK_ACCESS_INDEX_READ_BIT;
	} break;
	case BufferUsage::VertexBuffer: {
		access.stage  = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
		access.access = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	} break;
	case BufferUsage::DrawCommands: {
		access.stage  = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
		access.access = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
	} break;
	case BufferUsage::HostWrite: {
		access.stage  = VK_PIPELINE_STAGE_HOST_BIT;
		access.access = VK_ACCESS_HOST_WRITE_BIT;
	} break;
	case BufferUsage::None: {
		access.stage  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		access.access = 0;
	} break;
	default:
		ASSERT(false);
		break;
	};
	return access;
}

inline BufferAccess get_dst_buffer_access(BufferUsage usage)
{
	BufferAccess access;
	switch (usage) {
	case BufferUsage::GraphicsShaderRead: {
		access.stage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		access.access = VK_ACCESS_SHADER_READ_BIT;
	} break;
	case BufferUsage::GraphicsShaderReadWrite: {
		access.stage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		access.access = VK_ACCESS_SHADER_WRITE_BIT;
	} break;
	case BufferUsage::ComputeShaderRead: {
		access.stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		access.access = VK_ACCESS_SHADER_READ_BIT;
	} break;
	case BufferUsage::ComputeShaderReadWrite: {
		access.stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		access.access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	} break;
	case BufferUsage::TransferDst: {
		access.stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
		access.access = VK_ACCESS_TRANSFER_WRITE_BIT;
	} break;
	case BufferUsage::TransferSrc: {
		access.stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
		access.access = VK_ACCESS_TRANSFER_READ_BIT;
	} break;
	case BufferUsage::IndexBuffer: {
		access.stage  = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
		access.access = VK_ACCESS_INDEX_READ_BIT;
	} break;
	case BufferUsage::VertexBuffer: {
		access.stage  = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
		access.access = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	} break;
	case BufferUsage::DrawCommands: {
		access.stage  = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
		access.access = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
	} break;
	case BufferUsage::HostWrite: {
		access.stage  = VK_PIPELINE_STAGE_HOST_BIT;
		access.access = VK_ACCESS_HOST_WRITE_BIT;
	} break;
	case BufferUsage::None: {
		access.stage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		access.access = 0;
	} break;
	default:
		ASSERT(false);
		break;
	};
	return access;
}

inline VkBufferMemoryBarrier get_buffer_barrier(
	VkBuffer buffer, const BufferAccess &src, const BufferAccess &dst, usize offset, usize size)
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

inline bool is_depth_format(VkFormat format) { return format == VK_FORMAT_D32_SFLOAT; }

inline VkImageViewType view_type_from_image(VkImageType type)
{
	if (type == VK_IMAGE_TYPE_2D) {
		return VK_IMAGE_VIEW_TYPE_2D;
	}
	return VK_IMAGE_VIEW_TYPE_2D;
}

} // namespace vulkan
