#include "render/vulkan/device.h"

#include "render/vulkan/context.h"
#include "render/vulkan/framebuffer.h"
#include "render/vulkan/pipelines.h"
#include "render/vulkan/utils.h"

#include "exo/collections/array.h"
#include "exo/logger.h"

#include <vk_mem_alloc.h>

namespace vulkan
{

Device Device::create(const Context &context, const DeviceDescription &desc)
{
	Device device               = {};
	device.desc                 = desc;
	device.physical_device      = *desc.physical_device;
	device.push_constant_layout = desc.push_constant_layout;

	// Features warnings!
	if (!device.physical_device.vulkan12_features.timelineSemaphore) {
		exo::logger::error("This device does not support timeline semaphores from Vulkan 1.2");
	}

	if (!device.physical_device.vulkan12_features.bufferDeviceAddress) {
		exo::logger::error("This device does not support buffer device address from Vulkan 1.2");
	}

	if (desc.buffer_device_address == false &&
		device.physical_device.vulkan12_features.bufferDeviceAddress == VK_TRUE) {
		device.physical_device.vulkan12_features.bufferDeviceAddress = VK_FALSE;
	}

	device.physical_device.vulkan12_features.bufferDeviceAddressCaptureReplay = VK_FALSE;
	device.physical_device.vulkan12_features.bufferDeviceAddressMultiDevice   = VK_FALSE;

	/// --- Create the logical device
	uint installed_device_extensions_count = 0;
	vk_check(vkEnumerateDeviceExtensionProperties(device.physical_device.vkdevice,
		nullptr,
		&installed_device_extensions_count,
		nullptr));
	auto installed_device_extensions = Vec<VkExtensionProperties>::with_length(installed_device_extensions_count);
	vk_check(vkEnumerateDeviceExtensionProperties(device.physical_device.vkdevice,
		nullptr,
		&installed_device_extensions_count,
		installed_device_extensions.data()));

	Vec<const char *> device_extensions;
	device_extensions.push(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	device_extensions.push(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
	// device_extensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
	if (is_extension_installed(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME, installed_device_extensions)) {
		device_extensions.push(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME);
	}
	device_extensions.push(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);

	uint queue_families_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device.physical_device.vkdevice, &queue_families_count, nullptr);
	auto queue_families = Vec<VkQueueFamilyProperties>::with_length(queue_families_count);
	vkGetPhysicalDeviceQueueFamilyProperties(device.physical_device.vkdevice,
		&queue_families_count,
		queue_families.data());

	Vec<VkDeviceQueueCreateInfo> queue_create_infos;
	const float                  priority = 0.0;

	for (uint32_t i = 0; i < queue_families.len(); i++) {
		VkDeviceQueueCreateInfo queue_info = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
		queue_info.queueFamilyIndex        = i;
		queue_info.queueCount              = 1;
		queue_info.pQueuePriorities        = &priority;

		if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			if (device.graphics_family_idx == u32_invalid) {
				queue_create_infos.push(queue_info);
				device.graphics_family_idx = i;
			}
		} else if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
			if (device.compute_family_idx == u32_invalid && (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
				queue_create_infos.push(queue_info);
				device.compute_family_idx = i;
			}
		} else if (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
			if (device.transfer_family_idx == u32_invalid && (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT)) {
				queue_create_infos.push(queue_info);
				device.transfer_family_idx = i;
			}
		}
	}

	if (device.graphics_family_idx == u32_invalid) {
		exo::logger::error("Failed to find a graphics queue.\n");
	}
	if (device.compute_family_idx == u32_invalid) {
		exo::logger::error("Failed to find a compute queue.\n");
	}
	if (device.transfer_family_idx == u32_invalid) {
		exo::logger::error("Failed to find a transfer queue.\n");
		device.transfer_family_idx = device.compute_family_idx;
	}

	device.physical_device.features.pNext = &device.physical_device.vulkan12_features;

	VkDeviceCreateInfo dci      = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
	dci.pNext                   = &device.physical_device.features;
	dci.flags                   = 0;
	dci.queueCreateInfoCount    = static_cast<uint32_t>(queue_create_infos.len());
	dci.pQueueCreateInfos       = queue_create_infos.data();
	dci.enabledLayerCount       = 0;
	dci.ppEnabledLayerNames     = nullptr;
	dci.enabledExtensionCount   = static_cast<uint32_t>(device_extensions.len());
	dci.ppEnabledExtensionNames = device_extensions.data();
	dci.pEnabledFeatures        = nullptr;

	vk_check(vkCreateDevice(device.physical_device.vkdevice, &dci, nullptr, &device.device));
	volkLoadDevice(device.device);

	device.physical_device.features.pNext = nullptr;

	/// --- Init VMA allocator
	VmaVulkanFunctions vma_functions    = {};
	vma_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	vma_functions.vkGetDeviceProcAddr   = vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo allocator_info = {};
	allocator_info.vulkanApiVersion       = VK_API_VERSION_1_2;
	allocator_info.physicalDevice         = device.physical_device.vkdevice;
	allocator_info.device                 = device.device;
	allocator_info.instance               = context.instance;
	allocator_info.flags                  = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
	if (desc.buffer_device_address) {
		allocator_info.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	}
	allocator_info.pVulkanFunctions = &vma_functions;
	vk_check(vmaCreateAllocator(&allocator_info, &device.allocator));

	/// --- Descriptor sets
	{
		VkDescriptorPoolSize pool_sizes[] = {
			{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = 1024},
		};

		VkDescriptorPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
		pool_info.flags                      = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.poolSizeCount              = static_cast<u32>(exo::Array::len(pool_sizes));
		pool_info.pPoolSizes                 = pool_sizes;
		pool_info.maxSets                    = 1024;

		vk_check(
			vkCreateDescriptorPool(device.device, &pool_info, nullptr, &device.global_sets.uniform_descriptor_pool));

		device.global_sets.bindless = create_bindless_set(device, 1024, 1024, 32 * 1024);

		VkPushConstantRange push_constant_range;
		push_constant_range.stageFlags = VK_SHADER_STAGE_ALL;
		push_constant_range.offset     = 0;
		push_constant_range.size       = static_cast<u32>(device.push_constant_layout.size);

		// Create the dynamic buffer descriptor layout
		VkDescriptorSetLayoutBinding bindings[] = {{
			.binding         = 0,
			.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			.descriptorCount = 1,
			.stageFlags      = VK_SHADER_STAGE_ALL,
		}};

		const VkDescriptorSetLayoutCreateInfo desc_layout_info = {
			.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = static_cast<u32>(exo::Array::len(bindings)),
			.pBindings    = bindings,
		};

		vk_check(
			vkCreateDescriptorSetLayout(device.device, &desc_layout_info, nullptr, &device.global_sets.uniform_layout));

		VkDescriptorSetLayout layouts[] = {
			device.global_sets.bindless.vklayout,
			device.global_sets.uniform_layout,
			device.global_sets.uniform_layout,
		};

		VkPipelineLayoutCreateInfo pipeline_layout_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
		pipeline_layout_info.setLayoutCount             = static_cast<u32>(exo::Array::len(layouts));
		pipeline_layout_info.pSetLayouts                = layouts;
		pipeline_layout_info.pushConstantRangeCount     = push_constant_range.size ? 1 : 0;
		pipeline_layout_info.pPushConstantRanges        = &push_constant_range;

		vk_check(
			vkCreatePipelineLayout(device.device, &pipeline_layout_info, nullptr, &device.global_sets.pipeline_layout));
	}

	/// --- Create default samplers
	device.samplers.resize(BuiltinSampler::Count);
	VkSamplerCreateInfo sampler_info = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
	sampler_info.magFilter           = VK_FILTER_LINEAR;
	sampler_info.minFilter           = VK_FILTER_LINEAR;
	sampler_info.mipmapMode          = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_info.addressModeU        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeV        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeW        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.compareOp           = VK_COMPARE_OP_NEVER;
	sampler_info.borderColor         = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	sampler_info.minLod              = 0;
	sampler_info.maxLod              = 7;
	sampler_info.maxAnisotropy       = 8.0f;
	sampler_info.anisotropyEnable    = true;
	vk_check(vkCreateSampler(device.device, &sampler_info, nullptr, &device.samplers[BuiltinSampler::Default]));

	sampler_info.magFilter = VK_FILTER_NEAREST;
	sampler_info.minFilter = VK_FILTER_NEAREST;
	vk_check(vkCreateSampler(device.device, &sampler_info, nullptr, &device.samplers[BuiltinSampler::Nearest]));

	device.create_image(ImageDescription{.name = "empty image", .usages = storage_image_usage | sampled_image_usage});
	for (u32 i_slot = 1; i_slot < 1024; ++i_slot) {
		device.global_sets.bindless.pending_unbind[BindlessSet::PER_SAMPLER].push(i_slot);
		device.global_sets.bindless.pending_unbind[BindlessSet::PER_IMAGE].push(i_slot);
	}

	return device;
}

void Device::destroy(const Context & /*context*/)
{
	this->wait_idle();

	if (device == VK_NULL_HANDLE)
		return;

	for (auto [handle, _] : graphics_programs)
		destroy_program(handle);

	for (auto [handle, _] : compute_programs)
		destroy_program(handle);

	for (auto [handle, _] : shaders)
		destroy_shader(handle);

	for (auto [handle, _] : framebuffers)
		destroy_framebuffer(handle);

	for (auto [handle, _] : images)
		destroy_image(handle);

	for (auto [handle, _] : buffers)
		destroy_buffer(handle);

	for (auto sampler : samplers)
		vkDestroySampler(device, sampler, nullptr);

	vkDestroyDescriptorSetLayout(device, global_sets.uniform_layout, nullptr);
	vkDestroyDescriptorPool(device, global_sets.uniform_descriptor_pool, nullptr);
	destroy_bindless_set(*this, global_sets.bindless);
	vkDestroyPipelineLayout(device, global_sets.pipeline_layout, nullptr);

	vmaDestroyAllocator(allocator);
	vkDestroyDevice(device, nullptr);
}

/// --- Global descriptor set

void Device::update_globals() { update_bindless_set(*this, global_sets.bindless); }

const DynamicBufferDescriptor &Device::find_or_create_uniform_descriptor(Handle<Buffer> buffer_handle, usize size)
{
	for (const auto &descriptor : global_sets.uniform_descriptors) {
		if (descriptor.buffer == buffer_handle && descriptor.size == size) {
			return descriptor;
		}
	}

	global_sets.uniform_descriptors.push(create_buffer_descriptor(*this, buffer_handle, size));
	return global_sets.uniform_descriptors.last();
}

} // namespace vulkan
