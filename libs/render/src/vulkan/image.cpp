#include "render/vulkan/image.h"

#include "exo/format.h"
#include "render/vulkan/descriptor_set.h"
#include "render/vulkan/device.h"
#include "render/vulkan/utils.h"

#include <exo/memory/scope_stack.h>
#include <exo/string_view.h>
#include <vk_mem_alloc.h>

namespace vulkan
{

static ImageView create_image_view(Device &device,
	VkImage                                vkhandle,
	exo::StringView                        name,
	VkImageSubresourceRange               &range,
	VkFormat                               format,
	VkImageViewType                        type)
{
	ImageView view = {};

	view.vkhandle = VK_NULL_HANDLE;
	view.name     = name;
	view.range    = range;

	VkImageViewCreateInfo vci = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	vci.flags                 = 0;
	vci.image                 = vkhandle;
	vci.format                = format;
	vci.components.r          = VK_COMPONENT_SWIZZLE_IDENTITY;
	vci.components.g          = VK_COMPONENT_SWIZZLE_IDENTITY;
	vci.components.b          = VK_COMPONENT_SWIZZLE_IDENTITY;
	vci.components.a          = VK_COMPONENT_SWIZZLE_IDENTITY;
	vci.subresourceRange      = range;
	vci.viewType              = type;

	vk_check(vkCreateImageView(device.device, &vci, nullptr, &view.vkhandle));

	if (vkSetDebugUtilsObjectNameEXT) {
		VkDebugUtilsObjectNameInfoEXT ni = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
		ni.objectHandle                  = reinterpret_cast<u64>(view.vkhandle);
		ni.objectType                    = VK_OBJECT_TYPE_IMAGE_VIEW;
		ni.pObjectName                   = view.name.c_str();
		vk_check(vkSetDebugUtilsObjectNameEXT(device.device, &ni));
	}

	return view;
}

Handle<Image> Device::create_image(const ImageDescription &image_desc, Option<VkImage> proxy)
{
	const bool is_sampled = image_desc.usages & VK_IMAGE_USAGE_SAMPLED_BIT;
	const bool is_storage = image_desc.usages & VK_IMAGE_USAGE_STORAGE_BIT;
	const bool is_depth   = is_depth_format(image_desc.format);

	ASSERT(image_desc.size.x > 0);
	ASSERT(image_desc.size.y > 0);
	ASSERT(image_desc.size.z > 0);

	VkImageCreateInfo image_info     = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	image_info.imageType             = image_desc.type;
	image_info.format                = image_desc.format;
	image_info.extent.width          = static_cast<u32>(image_desc.size.x);
	image_info.extent.height         = static_cast<u32>(image_desc.size.y);
	image_info.extent.depth          = static_cast<u32>(image_desc.size.z);
	image_info.mipLevels             = image_desc.mip_levels;
	image_info.arrayLayers           = 1;
	image_info.samples               = image_desc.samples;
	image_info.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
	image_info.usage                 = image_desc.usages;
	image_info.queueFamilyIndexCount = 0;
	image_info.pQueueFamilyIndices   = nullptr;
	image_info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
	image_info.tiling                = VK_IMAGE_TILING_OPTIMAL;

	VkImage       vkhandle   = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;

	if (proxy) {
		vkhandle = *proxy;
	} else {
		VmaAllocationCreateInfo alloc_info{};
		alloc_info.flags     = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
		alloc_info.usage     = VmaMemoryUsage(image_desc.memory_usage);
		alloc_info.pUserData = const_cast<void *>(reinterpret_cast<const void *>(image_desc.name.c_str()));

		vk_check(vmaCreateImage(allocator, &image_info, &alloc_info, &vkhandle, &allocation, nullptr));
	}

	if (vkSetDebugUtilsObjectNameEXT) {
		VkDebugUtilsObjectNameInfoEXT ni = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
		ni.objectHandle                  = reinterpret_cast<u64>(vkhandle);
		ni.objectType                    = VK_OBJECT_TYPE_IMAGE;
		ni.pObjectName                   = image_desc.name.c_str();
		vk_check(vkSetDebugUtilsObjectNameEXT(device, &ni));
	}

	VkImageSubresourceRange full_range = {};
	full_range.aspectMask              = is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	full_range.baseMipLevel            = 0;
	full_range.levelCount              = image_info.mipLevels;
	full_range.baseArrayLayer          = 0;
	full_range.layerCount              = image_info.arrayLayers;
	const VkFormat format              = image_desc.format;

	exo::ScopeStack scope;
	const ImageView full_view = create_image_view(*this,
		vkhandle,
		exo::format(scope, "{} full view", image_desc.name),
		full_range,
		format,
		view_type_from_image(image_desc.type));

	auto handle = images.add({
		.desc       = image_desc,
		.vkhandle   = vkhandle,
		.allocation = allocation,
		.usage      = ImageUsage::None,
		.is_proxy   = proxy.has_value(),
		.full_view  = full_view,
	});

	// exo::logger::info("Creating image \"{}\" at index #{}\n", image_desc.name, handle.value());

	// Bindless (bind everything)
	if (is_sampled) {
		auto &image                 = images.get(handle);
		image.full_view.sampled_idx = bind_sampler_image(global_sets.bindless, handle);
	}

	if (is_storage) {
		auto &image                 = images.get(handle);
		image.full_view.storage_idx = bind_storage_image(global_sets.bindless, handle);
	}

	return handle;
}

void Device::destroy_image(Handle<Image> image_handle)
{
	auto &image = images.get(image_handle);
	this->unbind_image(image_handle);
	if (!image.is_proxy) {
		vmaDestroyImage(allocator, image.vkhandle, image.allocation);
	}
	vkDestroyImageView(device, image.full_view.vkhandle, nullptr);
	images.remove(image_handle);
}

int3 Device::get_image_size(Handle<Image> image_handle) { return this->images.get(image_handle).desc.size; }

u32 Device::get_image_sampled_index(Handle<Image> image_handle) const
{
	const u32 index = this->images.get(image_handle).full_view.sampled_idx;
	ASSERT(index != u32_invalid);
	return index;
}

u32 Device::get_image_storage_index(Handle<Image> image_handle) const
{
	const u32 index = this->images.get(image_handle).full_view.storage_idx;
	ASSERT(index != u32_invalid);
	return index;
}

void Device::unbind_image(Handle<Image> image_handle)
{
	auto &image = images.get(image_handle);
	if (image.full_view.sampled_idx != u32_invalid) {
		unbind_sampler_image(global_sets.bindless, image.full_view.sampled_idx);
		image.full_view.sampled_idx = u32_invalid;
	}
	if (image.full_view.storage_idx != u32_invalid) {
		unbind_storage_image(global_sets.bindless, image.full_view.storage_idx);
		image.full_view.storage_idx = u32_invalid;
	}
}
}; // namespace vulkan
