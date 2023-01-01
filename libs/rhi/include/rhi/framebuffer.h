#pragma once
#include "exo/collections/dynamic_array.h"
#include "exo/collections/handle.h"
#include "exo/maths/numerics.h"
#include "exo/maths/vectors.h"
#include "exo/option.h"

#include "rhi/operators.h"

namespace rhi
{
struct Image;
struct Device;

// Maximum number of attachments (color + depth) in a framebuffer
inline constexpr usize MAX_ATTACHMENTS = 4;
// Maximum number of renderpass (combination of load operator) per framebuffer
inline constexpr usize MAX_RENDERPASS = 4;
// Maximum number of render state per pipeline
inline constexpr usize MAX_RENDER_STATES = 4;

struct LoadOp
{
	enum struct Type
	{
		Load,
		Clear,
		Ignore
	};

	Type         type;
	VkClearValue color;

	static inline LoadOp load()
	{
		LoadOp load_op;
		load_op.type  = Type::Load;
		load_op.color = {};
		return load_op;
	}

	static inline LoadOp clear(VkClearValue color)
	{
		LoadOp load_op;
		load_op.type  = Type::Clear;
		load_op.color = color;
		return load_op;
	}

	static inline LoadOp ignore()
	{
		LoadOp load_op;
		load_op.type  = Type::Ignore;
		load_op.color = {};
		return load_op;
	}

	inline bool operator==(const LoadOp &other) const { return this->type == other.type && this->color == other.color; }
};

inline VkAttachmentLoadOp to_vk(LoadOp op)
{
	switch (op.type) {
	case LoadOp::Type::Load:
		return VK_ATTACHMENT_LOAD_OP_LOAD;
	case LoadOp::Type::Clear:
		return VK_ATTACHMENT_LOAD_OP_CLEAR;
	case LoadOp::Type::Ignore:
		return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	}
	return VK_ATTACHMENT_LOAD_OP_MAX_ENUM;
}

struct RenderPass
{
	VkRenderPass                               vkhandle;
	exo::DynamicArray<LoadOp, MAX_ATTACHMENTS> load_ops;
};

struct FramebufferFormat
{
	int3                                         size = int3(1, 1, 1);
	exo::DynamicArray<VkFormat, MAX_ATTACHMENTS> attachments_format;
	Option<VkFormat>                             depth_format;
	bool                                         operator==(const FramebufferFormat &) const = default;
};

struct Framebuffer
{
	VkFramebuffer                                     vkhandle = VK_NULL_HANDLE;
	FramebufferFormat                                 format;
	exo::DynamicArray<Handle<Image>, MAX_ATTACHMENTS> color_attachments;
	Handle<Image>                                     depth_attachment;

	exo::DynamicArray<RenderPass, MAX_RENDERPASS> renderpasses;

	bool operator==(const Framebuffer &) const = default;
};

RenderPass create_renderpass(Device &device, const FramebufferFormat &format, exo::Span<const LoadOp> load_ops);

} // namespace rhi
