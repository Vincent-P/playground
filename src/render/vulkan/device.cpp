#include "render/vulkan/device.hpp"

#include "base/types.hpp"
#include "base/logger.hpp"

#include "render/vulkan/descriptor_set.hpp"
#include "render/vulkan/utils.hpp"
#include "render/vulkan/surface.hpp"
#include "platform/window.hpp"
#include "vulkan/vulkan_core.h"

#include <array>

namespace vulkan
{

Device Device::create(const Context &context, const PhysicalDevice &physical_device)
{
    Device device = {};
    device.physical_device = physical_device;

    // Features warnings!
    if (!device.physical_device.vulkan12_features.timelineSemaphore)
    {
        logger::error("This device does not support timeline semaphores from Vulkan 1.2");
    }

    if (!device.physical_device.vulkan12_features.bufferDeviceAddress)
    {
        logger::error("This device does not support buffer device address from Vulkan 1.2");
    }

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
    if (is_extension_installed(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME, installed_device_extensions))
    {
        device_extensions.push_back(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME);
    }

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
    allocator_info.flags                  = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    VK_CHECK(vmaCreateAllocator(&allocator_info, &device.allocator));

    /// --- Descriptor sets pool
    {
    std::array pool_sizes{
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 16},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          .descriptorCount = 16},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = 16},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         .descriptorCount = 16},
    };

    VkDescriptorPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.flags                      = 0;
    pool_info.poolSizeCount              = pool_sizes.size();
    pool_info.pPoolSizes                 = pool_sizes.data();
    pool_info.maxSets                    = 16;

    VK_CHECK(vkCreateDescriptorPool(device.device, &pool_info, nullptr, &device.descriptor_pool));
    }

    /// --- Create default samplers
    device.samplers.resize(1);
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
    VK_CHECK(vkCreateSampler(device.device, &sampler_info, nullptr, &device.samplers[0]));

    /// --- Create the global descriptor set
    {
    std::array pool_sizes{
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1024},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          .descriptorCount = 1024},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = 1},
    };

    VkDescriptorPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.flags                      = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    pool_info.poolSizeCount              = pool_sizes.size();
    pool_info.pPoolSizes                 = pool_sizes.data();
    pool_info.maxSets                    = 1;

    VK_CHECK(vkCreateDescriptorPool(device.device, &pool_info, nullptr, &device.global_set.vkpool));


    Vec<VkDescriptorSetLayoutBinding> bindings = {
        {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount =    1, .stageFlags = VK_SHADER_STAGE_ALL},
        {.binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1024, .stageFlags = VK_SHADER_STAGE_ALL},
        {.binding = 2, .descriptorType =          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1024, .stageFlags = VK_SHADER_STAGE_ALL},
    };

    VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
        | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
        | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;

    Vec<VkDescriptorBindingFlags> bindings_flags = {
        0,
        flags,
        flags,
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
    flags_info.bindingCount  = bindings_flags.size();
    flags_info.pBindingFlags = bindings_flags.data();

    VkDescriptorSetLayoutCreateInfo desc_layout_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    desc_layout_info.pNext        = &flags_info;
    desc_layout_info.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    desc_layout_info.bindingCount = bindings.size();
    desc_layout_info.pBindings    = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(device.device, &desc_layout_info, nullptr, &device.global_set.vklayout));

    VkPipelineLayoutCreateInfo pipeline_layout_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts    = &device.global_set.vklayout;

    VK_CHECK(vkCreatePipelineLayout(device.device, &pipeline_layout_info, nullptr, &device.global_set.vkpipelinelayout));


    VkDescriptorSetAllocateInfo set_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    set_info.descriptorPool              = device.global_set.vkpool;
    set_info.pSetLayouts                 = &device.global_set.vklayout;
    set_info.descriptorSetCount          = 1;

    VK_CHECK(vkAllocateDescriptorSets(device.device, &set_info, &device.global_set.vkset));
    }

    return device;
}

void Device::destroy(const Context &context)
{
    UNUSED(context);

    this->wait_idle();

    if (device == VK_NULL_HANDLE)
        return;

    for (auto &[handle, _] : graphics_programs)
        destroy_program(handle);

    for (auto &[handle, _] : shaders)
        destroy_shader(handle);

    for (auto &[handle, _] : renderpasses)
        destroy_renderpass(handle);

    for (auto &[handle, _] : framebuffers)
        destroy_framebuffer(handle);

    for (auto &[handle, _] : images)
        destroy_image(handle);

    for (auto &[handle, _] : buffers)
        destroy_buffer(handle);

    for (auto sampler : samplers)
        vkDestroySampler(device, sampler, nullptr);

    vkDestroyDescriptorPool(device, descriptor_pool, nullptr);

    vkDestroyDescriptorSetLayout(device, global_set.vklayout, nullptr);
    vkDestroyDescriptorPool(device, global_set.vkpool, nullptr);
    vkDestroyPipelineLayout(device, global_set.vkpipelinelayout, nullptr);

    vmaDestroyAllocator(allocator);
    vkDestroyDevice(device, nullptr);
}

/// --- Global descriptor set

u32 Device::bind_global_storage_image(u32 /*index*/, Handle<Image> image_handle)
{
    global_set.storage_images.push_back(image_handle);
    u32 idx = global_set.storage_images.size();

    global_set.pending_images.push_back(image_handle);
    global_set.pending_indices.push_back(idx);
    global_set.pending_binding.push_back(2);
    return idx;
}

u32 Device::bind_global_sampled_image(u32 /*index*/, Handle<Image> image_handle)
{
    global_set.sampled_images.push_back(image_handle);
    u32 idx = global_set.storage_images.size();
    global_set.pending_images.push_back(image_handle);
    global_set.pending_indices.push_back(idx);
    global_set.pending_binding.push_back(1);
    return idx;
}

void Device::update_globals()
{
    Vec<VkWriteDescriptorSet> writes(global_set.pending_images.size());

    // writes' elements contain pointers to these buffers, so they have to be allocated with the right size
    Vec<VkDescriptorImageInfo> images_info;
    images_info.reserve(global_set.pending_images.size());

    for (uint i_pending = 0; i_pending < global_set.pending_images.size(); i_pending++)
    {
        writes[i_pending]                  = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[i_pending].dstSet           = global_set.vkset;
        writes[i_pending].dstBinding       = global_set.pending_binding[i_pending];
        writes[i_pending].dstArrayElement  = global_set.pending_indices[i_pending];
        writes[i_pending].descriptorCount  = 1;
        writes[i_pending].descriptorType   = writes[i_pending].dstBinding == 1 ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

        auto &image = *images.get(global_set.pending_images[i_pending]);
        images_info.push_back({
                .sampler = samplers[BuiltinSampler::Default],
                .imageView = image.full_view,
                .imageLayout = writes[i_pending].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL,
            });
        writes[i_pending].pImageInfo = &images_info.back();
    }

    vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
}

}
