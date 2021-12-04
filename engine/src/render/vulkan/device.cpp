#include "render/vulkan/device.h"

#include <exo/prelude.h>
#include <exo/base/logger.h>

#include "render/vulkan/bindless_set.h"
#include "render/vulkan/context.h"
#include "render/vulkan/descriptor_set.h"
#include "render/vulkan/utils.h"
#include "render/vulkan/surface.h"
#include <exo/cross/window.h>
#include "vulkan/vulkan_core.h"

#include <array>

namespace vulkan
{

Device Device::create(const Context &context, const DeviceDescription &desc)
{
    Device device = {};
    device.desc = desc;
    device.physical_device = *desc.physical_device;
    device.push_constant_layout = desc.push_constant_layout;

    // Features warnings!
    if (!device.physical_device.vulkan12_features.timelineSemaphore)
    {
        logger::error("This device does not support timeline semaphores from Vulkan 1.2");
    }

    if (!device.physical_device.vulkan12_features.bufferDeviceAddress)
    {
        logger::error("This device does not support buffer device address from Vulkan 1.2");
    }

    if (desc.buffer_device_address == false && device.physical_device.vulkan12_features.bufferDeviceAddress == VK_TRUE)
    {
        device.physical_device.vulkan12_features.bufferDeviceAddress = VK_FALSE;
    }

    device.physical_device.vulkan12_features.bufferDeviceAddressCaptureReplay = VK_FALSE;
    device.physical_device.vulkan12_features.bufferDeviceAddressMultiDevice   = VK_FALSE;

#define X(name) device.name = context.name
    X(vkCreateDebugUtilsMessengerEXT);
    X(vkDestroyDebugUtilsMessengerEXT);
    X(vkCmdBeginDebugUtilsLabelEXT);
    X(vkCmdEndDebugUtilsLabelEXT);
    X(vkSetDebugUtilsObjectNameEXT);
#undef X

    /// --- Create the logical device
    uint installed_device_extensions_count = 0;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(device.physical_device.vkdevice,
                                                  nullptr,
                                                  &installed_device_extensions_count,
                                                  nullptr));
    Vec<VkExtensionProperties> installed_device_extensions(installed_device_extensions_count);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(device.physical_device.vkdevice,
                                                  nullptr,
                                                  &installed_device_extensions_count,
                                                  installed_device_extensions.data()));

    Vec<const char *> device_extensions;
    device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    device_extensions.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
    device_extensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    if (is_extension_installed(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME, installed_device_extensions))
    {
        device_extensions.push_back(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME);
    }
    device_extensions.push_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);

    uint queue_families_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device.physical_device.vkdevice, &queue_families_count, nullptr);
    Vec<VkQueueFamilyProperties> queue_families(queue_families_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device.physical_device.vkdevice, &queue_families_count, queue_families.data());

    Vec<VkDeviceQueueCreateInfo> queue_create_infos;
    float priority = 0.0;

    for (uint32_t i = 0; i < queue_families.size(); i++)
    {
        VkDeviceQueueCreateInfo queue_info = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue_info.queueFamilyIndex = i;
        queue_info.queueCount       = 1;
        queue_info.pQueuePriorities = &priority;

        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            if (device.graphics_family_idx == u32_invalid)
            {
                queue_create_infos.push_back(queue_info);
                device.graphics_family_idx = i;
            }
        }
        else if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            if (device.compute_family_idx == u32_invalid && (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT))
            {
                queue_create_infos.push_back(queue_info);
                device.compute_family_idx = i;
            }
        }
        else if (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            if (device.transfer_family_idx == u32_invalid && (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT))
            {
                queue_create_infos.push_back(queue_info);
                device.transfer_family_idx = i;
            }
        }
    }

    if (device.graphics_family_idx == u32_invalid)
    {
        logger::error("Failed to find a graphics queue.\n");
    }
    if (device.compute_family_idx == u32_invalid)
    {
        logger::error("Failed to find a compute queue.\n");
    }
    if (device.transfer_family_idx == u32_invalid)
    {
        logger::error("Failed to find a transfer queue.\n");
        device.transfer_family_idx = device.compute_family_idx;
    }

    VkDeviceCreateInfo dci      = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.pNext                   = &device.physical_device.features;
    dci.flags                   = 0;
    dci.queueCreateInfoCount    = static_cast<uint32_t>(queue_create_infos.size());
    dci.pQueueCreateInfos       = queue_create_infos.data();
    dci.enabledLayerCount       = 0;
    dci.ppEnabledLayerNames     = nullptr;
    dci.enabledExtensionCount   = static_cast<uint32_t>(device_extensions.size());
    dci.ppEnabledExtensionNames = device_extensions.data();
    dci.pEnabledFeatures        = nullptr;

    VK_CHECK(vkCreateDevice(device.physical_device.vkdevice, &dci, nullptr, &device.device));

    /// --- Init VMA allocator
    VmaAllocatorCreateInfo allocator_info = {};
    allocator_info.vulkanApiVersion       = VK_API_VERSION_1_2;
    allocator_info.physicalDevice         = device.physical_device.vkdevice;
    allocator_info.device                 = device.device;
    allocator_info.instance               = context.instance;
    allocator_info.flags                  = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    if (desc.buffer_device_address)
    {
        allocator_info.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    }
    VK_CHECK(vmaCreateAllocator(&allocator_info, &device.allocator));

    /// --- Descriptor sets pool
    {
        std::array pool_sizes{
            VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1024},
            VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1024},
            VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = 1024},
            VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1024},
        };

        VkDescriptorPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pool_info.flags                      = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.poolSizeCount              = static_cast<u32>(pool_sizes.size());
        pool_info.pPoolSizes                 = pool_sizes.data();
        pool_info.maxSets                    = 1024;

        VK_CHECK(vkCreateDescriptorPool(device.device, &pool_info, nullptr, &device.descriptor_pool));
    }

    /// --- Create default samplers
    device.samplers.resize(BuiltinSampler::Count);
    VkSamplerCreateInfo sampler_info = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
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
    VK_CHECK(vkCreateSampler(device.device, &sampler_info, nullptr, &device.samplers[BuiltinSampler::Default]));

    sampler_info.magFilter = VK_FILTER_NEAREST;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    VK_CHECK(vkCreateSampler(device.device, &sampler_info, nullptr, &device.samplers[BuiltinSampler::Nearest]));

    /// --- Create the global descriptor sets
    {
        std::array pool_sizes{
            VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1024},
            VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          .descriptorCount = 1024},
            VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         .descriptorCount = 32 * 1024},
        };

        VkDescriptorPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pool_info.poolSizeCount              = static_cast<u32>(pool_sizes.size());
        pool_info.pPoolSizes                 = pool_sizes.data();
        pool_info.maxSets                    = 3;

        VK_CHECK(vkCreateDescriptorPool(device.device, &pool_info, nullptr, &device.global_sets.pool));

        device.global_sets.sampled_images  = create_bindless_set(device, device.global_sets.pool, "bindless sampled images",  DescriptorType{{{.count = 1024, .type = DescriptorType::SampledImage}}});
        device.global_sets.storage_images  = create_bindless_set(device, device.global_sets.pool, "bindless storage images",  DescriptorType{{{.count = 1024, .type = DescriptorType::StorageImage}}});
        device.global_sets.storage_buffers = create_bindless_set(device, device.global_sets.pool, "bindless storage buffers", DescriptorType{{{.count = 32 * 1024, .type = DescriptorType::StorageBuffer}}});
        device.global_sets.uniform         = create_descriptor_set(device, std::array{DescriptorType{{{.count = 1, .type = DescriptorType::DynamicBuffer}}}});

        VkPushConstantRange push_constant_range;
        push_constant_range.stageFlags = VK_SHADER_STAGE_ALL;
        push_constant_range.offset     = 0;
        push_constant_range.size       = static_cast<u32>(device.push_constant_layout.size);

        VkDescriptorSetLayout layouts[] = {
            device.global_sets.uniform.layout,
            device.global_sets.sampled_images.layout,
            device.global_sets.storage_images.layout,
            device.global_sets.storage_buffers.layout,
        };

        VkPipelineLayoutCreateInfo pipeline_layout_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipeline_layout_info.setLayoutCount             = ARRAY_SIZE(layouts);
        pipeline_layout_info.pSetLayouts                = layouts;
        pipeline_layout_info.pushConstantRangeCount     = push_constant_range.size ? 1 : 0;
        pipeline_layout_info.pPushConstantRanges        = &push_constant_range;

        VK_CHECK(vkCreatePipelineLayout(device.device, &pipeline_layout_info, nullptr, &device.global_sets.pipeline_layout));
    }

    return device;
}

void Device::destroy(const Context &context)
{
    UNUSED(context);

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

    destroy_descriptor_set(*this, global_sets.uniform);
    vkDestroyDescriptorPool(device, descriptor_pool, nullptr);

    destroy_bindless_set(*this, global_sets.sampled_images);
    destroy_bindless_set(*this, global_sets.storage_images);
    destroy_bindless_set(*this, global_sets.storage_buffers);
    vkDestroyDescriptorPool(device, global_sets.pool, nullptr);
    vkDestroyPipelineLayout(device, global_sets.pipeline_layout, nullptr);

    vmaDestroyAllocator(allocator);
    vkDestroyDevice(device, nullptr);
}

/// --- Global descriptor set

void Device::bind_global_uniform_buffer(Handle<Buffer> buffer_handle, usize offset, usize range)
{
    bind_uniform_buffer(global_sets.uniform, 0, buffer_handle, offset, range);
}

void Device::update_globals()
{
    update_bindless_set(*this, global_sets.sampled_images);
    update_bindless_set(*this, global_sets.storage_images);
    update_bindless_set(*this, global_sets.storage_buffers);
}

}
