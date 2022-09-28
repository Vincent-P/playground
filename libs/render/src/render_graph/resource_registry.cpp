#include "render/render_graph/resource_registry.h"

#include <exo/collections/dynamic_array.h>
#include <exo/collections/handle_map.h>
#include <exo/profile.h>

#include "render/vulkan/device.h"
#include <render/vulkan/framebuffer.h>
#include <render/vulkan/image.h>
#include <render/vulkan/utils.h>

void ResourceRegistry::begin_frame(vulkan::Device &device, u64 frame)
{
	this->i_frame = frame;

	Vec<Handle<vulkan::Image>> img_to_remove;
	for (auto [image_handle_b, metadata_handle_b] : this->image_pool) {
		auto image_handle = exo::as_handle<vulkan::Image>(image_handle_b);

		const auto &metadata = this->image_metadatas.get(exo::as_handle<ImageMetadata>(metadata_handle_b));

		// Unbind images from the bindless set unused for 18 frames
		if ((metadata.last_frame_used + 18) < this->i_frame) {
			device.unbind_image(image_handle);
		}

		// Destroy images unused for 19 frames
		if ((metadata.last_frame_used + 19) < this->i_frame) {
			img_to_remove.push_back(image_handle);
		}
	}

	Vec<Handle<vulkan::Framebuffer>> fb_to_remove;
	for (auto [fb_handle_b, metadata_handle_b] : this->framebuffer_pool) {
		const auto &metadata = this->framebuffer_metadatas.get(exo::as_handle<FramebufferMetadata>(metadata_handle_b));
		if (metadata.last_frame_used + 3 < this->i_frame) {
			fb_to_remove.push_back(exo::as_handle<vulkan::Framebuffer>(fb_handle_b));
		}
	}

	for (const auto handle_to_remove : fb_to_remove) {
		device.destroy_framebuffer(handle_to_remove);

		for (usize i_fb = 0; i_fb < this->framebuffers.size(); ++i_fb) {
			if (this->framebuffers[i_fb] == handle_to_remove) {
				exo::swap_remove(this->framebuffers, i_fb);
				this->framebuffer_pool.remove(exo::to_u64(handle_to_remove));
				break;
			}
		}
	}

	for (const auto handle_to_remove : img_to_remove) {
		device.destroy_image(handle_to_remove);
		this->image_pool.remove(exo::to_u64(handle_to_remove));
	}

	EXO_PROFILE_PLOT_VALUE("Graph: texture descs", i64(this->texture_descs.size));
}

void ResourceRegistry::end_frame()
{
	this->texture_descs.clear();
	for (auto [image, metadata_handle_b] : this->image_pool) {
		auto metadata_handle                                     = exo::as_handle<ImageMetadata>(metadata_handle_b);
		this->image_metadatas.get(metadata_handle).resolved_desc = Handle<TextureDesc>::invalid();
	}
}

static void update_image_metadata(ResourceRegistry &registry, Handle<vulkan::Image> image, Handle<TextureDesc> desc)
{
	ASSERT(image.is_valid());
	if (auto handle = registry.image_pool.at(exo::to_u64(image)); handle) {
		auto &metadata           = registry.image_metadatas.get(exo::as_handle<ImageMetadata>(handle.value()));
		metadata.resolved_desc   = desc;
		metadata.last_frame_used = registry.i_frame;
	} else {
		auto metadata_handle =
			registry.image_metadatas.add(ImageMetadata{.resolved_desc = desc, .last_frame_used = registry.i_frame});
		registry.image_pool.insert(exo::to_u64(image), exo::to_u64(metadata_handle));
	}
}

void ResourceRegistry::set_image(Handle<TextureDesc> desc_handle, Handle<vulkan::Image> image_handle)
{
	auto &desc          = this->texture_descs.get(desc_handle);
	desc.resolved_image = image_handle;
	update_image_metadata(*this, image_handle, desc_handle);
}

void ResourceRegistry::drop_image(Handle<vulkan::Image> image_handle)
{
	for (auto [handle, desc] : this->texture_descs) {
		if (desc->resolved_image == image_handle) {
			this->texture_descs.get(handle).resolved_image = Handle<vulkan::Image>::invalid();
		}
	}
	this->image_pool.remove(exo::to_u64(image_handle));
}

Handle<vulkan::Image> ResourceRegistry::resolve_image(vulkan::Device &device, Handle<TextureDesc> desc_handle)
{
	const auto &desc = this->texture_descs.get(desc_handle);

	Handle<vulkan::Image> resolved_image_handle = desc.resolved_image;
	if (resolved_image_handle.is_valid() == false) {

		VkImageUsageFlags usages = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		                           VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
		                           VK_IMAGE_USAGE_STORAGE_BIT;
		if (vulkan::is_depth_format(desc.format)) {
			usages = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}

		auto desc_spec = vulkan::ImageDescription{
			.name   = desc.name,
			.size   = int3(this->texture_desc_handle_size(desc_handle), 1),
			.type   = desc.image_type,
			.format = desc.format,
			.usages = usages,
		};

		for (auto [image_handle_b, metadata_handle_b] : this->image_pool) {
			auto        image_handle = exo::as_handle<vulkan::Image>(image_handle_b);
			const auto &metadata     = this->image_metadatas.get(exo::as_handle<ImageMetadata>(metadata_handle_b));

			if (!metadata.resolved_desc.is_valid()) {
				const auto &image = device.images.get(image_handle);
				if (image.desc == desc_spec) {
					resolved_image_handle = image_handle;
					break;
				}
			}
		}

		if (!resolved_image_handle.is_valid()) {
			resolved_image_handle = device.create_image(desc_spec);
			device.update_globals();
		}

		this->texture_descs.get(desc_handle).resolved_image = resolved_image_handle;
	}
	update_image_metadata(*this, resolved_image_handle, desc_handle);
	return resolved_image_handle;
}

int2 ResourceRegistry::texture_desc_handle_size(Handle<TextureDesc> desc_handle)
{
	const auto texture_size = this->texture_descs.get(desc_handle).size;
	switch (texture_size.type) {
	case TextureSizeType::Absolute:
		return texture_size.size.int2;
	case TextureSizeType::ScreenRelative:
		return int2(this->screen_size * texture_size.size.float2);
	default:
		ASSERT(false);
		return {};
	}
}

static void update_framebuffer_metadata(ResourceRegistry &registry, Handle<vulkan::Framebuffer> framebuffer)
{
	ASSERT(framebuffer.is_valid());
	if (auto handle = registry.framebuffer_pool.at(exo::to_u64(framebuffer)); handle) {
		auto &metadata = registry.framebuffer_metadatas.get(exo::as_handle<FramebufferMetadata>(handle.value()));
		metadata.last_frame_used = registry.i_frame;
	} else {
		auto metadata_handle =
			registry.framebuffer_metadatas.add(FramebufferMetadata{.last_frame_used = registry.i_frame});
		registry.framebuffer_pool.insert(exo::to_u64(framebuffer), exo::to_u64(metadata_handle));
	}
}

Handle<vulkan::Framebuffer> ResourceRegistry::resolve_framebuffer(vulkan::Device &device,
	std::span<const Handle<TextureDesc>>                                          color_attachments,
	Handle<TextureDesc>                                                           depth_attachment)
{
	exo::DynamicArray<Handle<vulkan::Image>, vulkan::MAX_ATTACHMENTS> color_images;
	for (auto desc_handle : color_attachments) {
		color_images.push_back(this->texture_descs.get(desc_handle).resolved_image);
	}

	Handle<vulkan::Image> depth_image = {};
	if (depth_attachment.is_valid()) {
		depth_image = this->resolve_image(device, depth_attachment);
	}

	int3 size;
	if (!color_images.empty()) {
		size = device.images.get(color_images[0]).desc.size;
	} else {
		ASSERT(depth_image.is_valid());
		size = device.images.get(depth_image).desc.size;
	}

	for (auto framebuffer_handle : this->framebuffers) {
		const auto &framebuffer = device.framebuffers.get(framebuffer_handle);
		if (framebuffer.color_attachments == color_images && framebuffer.depth_attachment == depth_image &&
			framebuffer.format.size == size) {
			update_framebuffer_metadata(*this, framebuffer_handle);
			return framebuffer_handle;
		}
	}

	auto new_handle = device.create_framebuffer(size, color_images, depth_image);
	update_framebuffer_metadata(*this, new_handle);
	this->framebuffers.push_back(new_handle);
	return new_handle;
}
