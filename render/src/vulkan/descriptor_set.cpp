#include "render/vulkan/descriptor_set.h"

#include <exo/collections/array.h>
#include <exo/memory/linear_allocator.h>
#include <exo/memory/scope_stack.h>

#include "render/vulkan/device.h"
#include "render/vulkan/utils.h"

#include <vulkan/vulkan_core.h>

namespace vulkan
{
/// --- Dynamic buffer descriptor

DynamicBufferDescriptor create_buffer_descriptor(Device &device, Handle<Buffer> &buffer_handle, usize range_size)
{
	auto &buffer = device.buffers.get(buffer_handle);

	VkDescriptorSetAllocateInfo set_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	set_info.descriptorPool              = device.global_sets.uniform_descriptor_pool;
	set_info.pSetLayouts                 = &device.global_sets.uniform_layout;
	set_info.descriptorSetCount          = 1;

	VkDescriptorSet vkset = VK_NULL_HANDLE;
	vk_check(vkAllocateDescriptorSets(device.device, &set_info, &vkset));

	VkDescriptorBufferInfo buffer_infos[] = {
		{.buffer = buffer.vkhandle, .offset = 0, .range = range_size},
	};
	VkWriteDescriptorSet writes[] = {
		{
			.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet          = vkset,
			.descriptorCount = 1,
			.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			.pBufferInfo     = &buffer_infos[0],
		},
	};

	vkUpdateDescriptorSets(device.device, static_cast<u32>(exo::Array::len(writes)), writes, 0, nullptr);

	return DynamicBufferDescriptor{
		.buffer = buffer_handle,
		.vkset  = vkset,
		.size   = static_cast<u32>(range_size),
	};
}

void destroy_buffer_descriptor(Device &device, DynamicBufferDescriptor &descriptor)
{
	vkFreeDescriptorSets(device.device, device.global_sets.uniform_descriptor_pool, 1, &descriptor.vkset);
}

/// --- Bindless set

BindlessSet create_bindless_set(const Device &device, u32 sampler_count, u32 image_count, u32 buffer_count)
{
	BindlessSet bindless = {};

	VkDescriptorPoolSize pool_sizes[] = {
		{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = sampler_count},
		{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = image_count},
		{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = buffer_count},
	};

	VkDescriptorPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	pool_info.flags                      = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	pool_info.poolSizeCount              = static_cast<u32>(exo::Array::len(pool_sizes));
	pool_info.pPoolSizes                 = pool_sizes;
	pool_info.maxSets                    = 3;

	vk_check(vkCreateDescriptorPool(device.device, &pool_info, nullptr, &bindless.vkpool));

	VkDescriptorSetLayoutBinding bindings[] = {
		{
			.binding         = 0,
			.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = sampler_count,
			.stageFlags      = VK_SHADER_STAGE_ALL,
		},
		{
			.binding         = 1,
			.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.descriptorCount = image_count,
			.stageFlags      = VK_SHADER_STAGE_ALL,
		},
		{
			.binding         = 2,
			.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = buffer_count,
			.stageFlags      = VK_SHADER_STAGE_ALL,
		},
	};

	VkDescriptorBindingFlags flags[exo::Array::len(bindings)] = {};
	for (usize i = 0; i < exo::Array::len(bindings); i += 1) {
		flags[i] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
		           VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
	}

	VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info = {
		.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		.bindingCount  = static_cast<u32>(exo::Array::len(bindings)),
		.pBindingFlags = flags,
	};

	VkDescriptorSetLayoutCreateInfo desc_layout_info = {
		.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext        = &flags_info,
		.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
		.bindingCount = static_cast<u32>(exo::Array::len(bindings)),
		.pBindings    = bindings,
	};

	vk_check(vkCreateDescriptorSetLayout(device.device, &desc_layout_info, nullptr, &bindless.vklayout));

	VkDescriptorSetAllocateInfo set_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	set_info.descriptorPool              = bindless.vkpool;
	set_info.pSetLayouts                 = &bindless.vklayout;
	set_info.descriptorSetCount          = 1;
	vk_check(vkAllocateDescriptorSets(device.device, &set_info, &bindless.vkset));

	if (vkSetDebugUtilsObjectNameEXT) {
		VkDebugUtilsObjectNameInfoEXT ni = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
		ni.objectHandle                  = reinterpret_cast<u64>(bindless.vkset);
		ni.objectType                    = VK_OBJECT_TYPE_DESCRIPTOR_SET;
		ni.pObjectName                   = "Bindless descriptor set";
		vk_check(vkSetDebugUtilsObjectNameEXT(device.device, &ni));
	}

	auto init_freelist = [](Vec<u32> &list, u32 count) {
		list.resize(count);
		for (u32 i = 0; i < count; ++i) {
			list[i] = count - i;
		}
		list[0] = u32_invalid;
	};

	init_freelist(bindless.free_list[BindlessSet::PER_SAMPLER], sampler_count);
	init_freelist(bindless.free_list[BindlessSet::PER_IMAGE], image_count);
	init_freelist(bindless.free_list[BindlessSet::PER_BUFFER], buffer_count);

	bindless.sampler_images.resize(sampler_count);
	bindless.storage_images.resize(image_count);
	bindless.storage_buffers.resize(buffer_count);

	return bindless;
}

void destroy_bindless_set(const Device &device, BindlessSet &bindless)
{
	vkDestroyDescriptorPool(device.device, bindless.vkpool, nullptr);
	vkDestroyDescriptorSetLayout(device.device, bindless.vklayout, nullptr);
	bindless.free_list[BindlessSet::PER_SAMPLER].clear();
	bindless.free_list[BindlessSet::PER_IMAGE].clear();
	bindless.free_list[BindlessSet::PER_BUFFER].clear();
}

u32 bind_sampler_image(BindlessSet &bindless, Handle<Image> image_handle)
{
	u32 new_index = bindless.free_list[BindlessSet::PER_SAMPLER].back();
	bindless.free_list[BindlessSet::PER_SAMPLER].pop_back();
	bindless.sampler_images[new_index] = image_handle;
	bindless.pending_bind[BindlessSet::PER_SAMPLER].push_back(new_index);
	return new_index;
}

u32 bind_storage_image(BindlessSet &bindless, Handle<Image> image_handle)
{
	u32 new_index = bindless.free_list[BindlessSet::PER_IMAGE].back();
	bindless.free_list[BindlessSet::PER_IMAGE].pop_back();
	bindless.storage_images[new_index] = image_handle;
	bindless.pending_bind[BindlessSet::PER_IMAGE].push_back(new_index);
	return new_index;
}

u32 bind_storage_buffer(BindlessSet &bindless, Handle<Buffer> buffer_handle)
{
	u32 new_index = bindless.free_list[BindlessSet::PER_BUFFER].back();
	bindless.free_list[BindlessSet::PER_BUFFER].pop_back();
	bindless.storage_buffers[new_index] = buffer_handle;
	bindless.pending_bind[BindlessSet::PER_BUFFER].push_back(new_index);
	return new_index;
}

void unbind_sampler_image(BindlessSet &set, u32 index)
{
	set.sampler_images[index] = {};
	set.free_list[BindlessSet::PER_SAMPLER].push_back(index);
	set.pending_unbind[BindlessSet::PER_SAMPLER].push_back(index);
}

void unbind_storage_image(BindlessSet &set, u32 index)
{
	set.storage_images[index] = {};
	set.free_list[BindlessSet::PER_IMAGE].push_back(index);
	set.pending_unbind[BindlessSet::PER_IMAGE].push_back(index);
}

void unbind_storage_buffer(BindlessSet &set, u32 index)
{
	set.storage_buffers[index] = {};
	set.free_list[BindlessSet::PER_BUFFER].push_back(index);
	set.pending_unbind[BindlessSet::PER_BUFFER].push_back(index);
}

void update_bindless_set(Device &device, BindlessSet &bindless)
{
	auto tmp_scope = exo::ScopeStack::with_allocator(&exo::tls_allocator);

	// Allocate memory for descriptor writes and copies
	usize total_bind_count = bindless.pending_bind[BindlessSet::PER_SAMPLER].size() +
	                         bindless.pending_bind[BindlessSet::PER_IMAGE].size() +
	                         bindless.pending_bind[BindlessSet::PER_BUFFER].size();
	usize total_unbind_count = bindless.pending_unbind[BindlessSet::PER_SAMPLER].size() +
	                           bindless.pending_unbind[BindlessSet::PER_IMAGE].size() +
	                           bindless.pending_unbind[BindlessSet::PER_BUFFER].size();

	auto *descriptor_writes       = tmp_scope.allocate<VkWriteDescriptorSet>(u32(total_bind_count));
	auto *descriptor_copies       = tmp_scope.allocate<VkCopyDescriptorSet>(u32(total_unbind_count));
	auto *descriptor_writes_begin = descriptor_writes;
	auto *descriptor_copies_begin = descriptor_copies;

	// Allocate memory to hold image and buffer descriptor infos
	usize total_image_info_count =
		bindless.pending_bind[BindlessSet::PER_SAMPLER].size() + bindless.pending_bind[BindlessSet::PER_IMAGE].size();
	usize total_buffer_info_count = bindless.pending_bind[BindlessSet::PER_BUFFER].size();

	VkDescriptorImageInfo *image_infos =
		total_image_info_count > 0 ? tmp_scope.allocate<VkDescriptorImageInfo>(u32(total_image_info_count)) : nullptr;
	VkDescriptorBufferInfo *buffer_infos =
		total_buffer_info_count > 0 ? tmp_scope.allocate<VkDescriptorBufferInfo>(u32(total_buffer_info_count))
									: nullptr;

	VkDescriptorType descriptor_types[] = {
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	};

	for (u32 i_set = 0; i_set < exo::Array::len(bindless.pending_bind); i_set += 1) {
		auto &pending_binds   = bindless.pending_bind[i_set];
		auto &pending_unbinds = bindless.pending_unbind[i_set];

		VkImageLayout img_layout = descriptor_types[i_set] == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
		                               ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		                               : VK_IMAGE_LAYOUT_GENERAL;

		for (auto to_bind : pending_binds) {
			auto &write           = *descriptor_writes++;
			write                 = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
			write.dstSet          = bindless.vkset;
			write.dstBinding      = i_set;
			write.dstArrayElement = to_bind;
			write.descriptorCount = 1;
			write.descriptorType  = descriptor_types[i_set];

			switch (i_set) {
			case BindlessSet::PER_SAMPLER: {
				auto        image_handle = bindless.sampler_images[to_bind];
				const auto &image        = device.images.get(image_handle);
				*image_infos             = {
								.sampler     = device.samplers[BuiltinSampler::Default],
								.imageView   = image.full_view.vkhandle,
								.imageLayout = img_layout,
                };
				write.pImageInfo = image_infos++;
				break;
			}
			case BindlessSet::PER_IMAGE: {
				auto        image_handle = bindless.storage_images[to_bind];
				const auto &image        = device.images.get(image_handle);
				*image_infos             = {
								.sampler     = device.samplers[BuiltinSampler::Default],
								.imageView   = image.full_view.vkhandle,
								.imageLayout = img_layout,
                };
				write.pImageInfo = image_infos++;
				break;
			}
			case BindlessSet::PER_BUFFER: {
				auto        buffer_handle = bindless.storage_buffers[to_bind];
				const auto &buffer        = device.buffers.get(buffer_handle);
				*buffer_infos             = {
								.buffer = buffer.vkhandle,
								.offset = 0,
								.range  = buffer.desc.size,
                };
				write.pBufferInfo = buffer_infos++;
				break;
			}
			default: {
				ASSERT(false);
			}
			}
		}

		// Copy descriptor #0 (null, empty image) to unbind
		for (auto to_unbind : pending_unbinds) {
			bool already_bound = false;
			for (auto to_bind : pending_binds) {
				if (to_unbind == to_bind) {
					already_bound = true;
					break;
				}
			}
			if (already_bound) {
				continue;
			}

			auto &copy           = *descriptor_copies++;
			copy                 = {.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET};
			copy.srcSet          = bindless.vkset;
			copy.srcBinding      = i_set;
			copy.srcArrayElement = 0;
			copy.dstSet          = bindless.vkset;
			copy.dstBinding      = i_set;
			copy.dstArrayElement = to_unbind;
			copy.descriptorCount = 1;
		}

		pending_binds.clear();
		pending_unbinds.clear();
	}

	u32 write_count = static_cast<u32>(descriptor_writes - descriptor_writes_begin);
	u32 copy_count  = static_cast<u32>(descriptor_copies - descriptor_copies_begin);
	vkUpdateDescriptorSets(device.device, write_count, descriptor_writes_begin, copy_count, descriptor_copies_begin);
}

Handle<Image> get_sampler_image_at(BindlessSet &set, u32 index) { return set.sampler_images[index]; }

Handle<Image> get_storage_image_at(BindlessSet &set, u32 index) { return set.storage_images[index]; }

Handle<Buffer> get_storage_buffer_at(BindlessSet &set, u32 index) { return set.storage_buffers[index]; }

} // namespace vulkan
