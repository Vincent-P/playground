#pragma once
#include <exo/maths/vectors.h>

#include "render/vulkan/operators.h"
#include "render/vulkan/queues.h"

#include <vk_mem_alloc.h>
#include <volk.h>

// TODO interned strings
#include <string>

namespace vulkan
{

inline constexpr VkImageUsageFlags depth_attachment_usage =
	VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
inline constexpr VkImageUsageFlags sampled_image_usage =
	VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
inline constexpr VkImageUsageFlags storage_image_usage =
	VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
inline constexpr VkImageUsageFlags color_attachment_usage =
	storage_image_usage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
	VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

struct ImageAccess
{
	VkPipelineStageFlags stage  = 0;
	VkAccessFlags        access = 0;
	VkImageLayout        layout = VK_IMAGE_LAYOUT_UNDEFINED;
	QueueType            queue  = QueueType::Graphics;
};

enum struct ImageUsage
{
	None,
	GraphicsShaderRead,
	GraphicsShaderReadWrite,
	ComputeShaderRead,
	ComputeShaderReadWrite,
	TransferDst,
	TransferSrc,
	ColorAttachment,
	DepthAttachment,
	Present
};

struct ImageDescription
{
	std::string           name                                        = "No name";
	int3                  size                                        = {1, 1, 1};
	u32                   mip_levels                                  = 1;
	VkImageType           type                                        = VK_IMAGE_TYPE_2D;
	VkFormat              format                                      = VK_FORMAT_R8G8B8A8_UNORM;
	VkSampleCountFlagBits samples                                     = VK_SAMPLE_COUNT_1_BIT;
	VkImageUsageFlags     usages                                      = sampled_image_usage;
	VmaMemoryUsage        memory_usage                                = VMA_MEMORY_USAGE_GPU_ONLY;
	bool                  operator==(const ImageDescription &b) const = default;
};

struct ImageView
{
	VkImageSubresourceRange range;
	VkImageView             vkhandle;
	u32                     sampled_idx = u32_invalid;
	u32                     storage_idx = u32_invalid;
	VkFormat                format;
	std::string             name;

	bool operator==(const ImageView &b) const = default;
};

struct Image
{
	ImageDescription desc;
	VkImage          vkhandle;
	VmaAllocation    allocation;
	ImageUsage       usage    = ImageUsage::None;
	bool             is_proxy = false;
	ImageView        full_view;
	ImageView        color_view;
	bool             operator==(const Image &b) const = default;
};

} // namespace vulkan
