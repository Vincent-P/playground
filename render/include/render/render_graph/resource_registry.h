#pragma once
#include <exo/collections/handle.h>
#include <exo/collections/index_map.h>
#include <exo/collections/pool.h>
#include <exo/collections/vector.h>
#include <exo/maths/vectors.h>

#include <span>
#include <string>
#include <string_view>
#include <volk.h>

namespace vulkan
{
struct Image;
struct Framebuffer;
struct Device;
} // namespace vulkan

enum struct TextureSizeType
{
	ScreenRelative,
	Absolute
};

struct TextureSize
{
	TextureSizeType type;
	union TextureSizeValue
	{
		float2 float2;
		int2   int2;
	} size;

	static constexpr TextureSize screen_relative(float2 size)
	{
		return TextureSize{
			.type = TextureSizeType::ScreenRelative,
			.size = TextureSizeValue{.float2 = size},
		};
	}
};

struct TextureDesc
{
	std::string           name           = "unnamed texture desc";
	TextureSize           size           = TextureSize::screen_relative(float2(1.0));
	VkFormat              format         = VK_FORMAT_R8G8B8A8_UNORM;
	VkImageType           image_type     = VK_IMAGE_TYPE_2D;
	Handle<vulkan::Image> resolved_image = {};
};

struct ImageMetadata
{
	Handle<TextureDesc> resolved_desc   = {};
	u64                 last_frame_used = 0;
};

struct FramebufferMetadata
{
	u64 last_frame_used = 0;
};

struct ResourceRegistry
{
	exo::Pool<TextureDesc>   texture_descs;
	exo::Pool<ImageMetadata> image_metadatas;
	exo::IndexMap            image_pool;

	Vec<Handle<vulkan::Framebuffer>> framebuffers;
	exo::Pool<FramebufferMetadata>   framebuffer_metadatas;
	exo::IndexMap                    framebuffer_pool;

	float2 screen_size = float2(1.0);
	u64    i_frame     = 0;

	void begin_frame(vulkan::Device &device, u64 frame);
	void end_frame();

	void                  set_image(Handle<TextureDesc> desc_handle, Handle<vulkan::Image> image_handle);
	void                  drop_image(Handle<vulkan::Image> image_handle);
	Handle<vulkan::Image> resolve_image(vulkan::Device &device, Handle<TextureDesc> desc_handle);

	int2 texture_desc_handle_size(Handle<TextureDesc> desc_handle);

	Handle<vulkan::Framebuffer> resolve_framebuffer(vulkan::Device &device,
		std::span<const Handle<TextureDesc>>                        color_attachments,
		Handle<TextureDesc>                                         depth_attachment);
};
