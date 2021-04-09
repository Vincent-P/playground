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
    allocator_info.flags                  = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    if (desc.buffer_device_address)
    {
        allocator_info.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    }
    VK_CHECK(vmaCreateAllocator(&allocator_info, &device.allocator));

    /// --- Descriptor sets pool
    {
    std::array pool_sizes{
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 128},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          .descriptorCount = 128},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = 128},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         .descriptorCount = 128},
    };

    VkDescriptorPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.flags                      = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.poolSizeCount              = pool_sizes.size();
    pool_info.pPoolSizes                 = pool_sizes.data();
    pool_info.maxSets                    = 128;

    VK_CHECK(vkCreateDescriptorPool(device.device, &pool_info, nullptr, &device.descriptor_pool));
    }

    /// --- Create default samplers
    device.samplers.resize(1);
    VkSamplerCreateInfo sampler_info = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    sampler_info.magFilter           = VK_FILTER_NEAREST;
    sampler_info.minFilter           = VK_FILTER_NEAREST;
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
        u32 storages_count = 1024;
        u32 sampled_count = 1024;
        std::array pool_sizes{
            VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = storages_count},
            VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          .descriptorCount = sampled_count},
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

        VkPushConstantRange push_constant_range;
        push_constant_range.stageFlags = VK_SHADER_STAGE_ALL;
        push_constant_range.offset     = 0;
        push_constant_range.size       = device.push_constant_layout.size;

        VkPipelineLayoutCreateInfo pipeline_layout_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipeline_layout_info.setLayoutCount             = 1;
        pipeline_layout_info.pSetLayouts                = &device.global_set.vklayout;
        pipeline_layout_info.pushConstantRangeCount     = push_constant_range.size ? 1 : 0;
        pipeline_layout_info.pPushConstantRanges        = &push_constant_range;

        VK_CHECK(vkCreatePipelineLayout(device.device, &pipeline_layout_info, nullptr, &device.global_set.vkpipelinelayout));

        VkDescriptorSetAllocateInfo set_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        set_info.descriptorPool              = device.global_set.vkpool;
        set_info.pSetLayouts                 = &device.global_set.vklayout;
        set_info.descriptorSetCount          = 1;

        VK_CHECK(vkAllocateDescriptorSets(device.device, &set_info, &device.global_set.vkset));

        device.global_set.storage_images.resize(storages_count);
        device.global_set.sampled_images.resize(sampled_count);
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

    for (auto &[handle, _] : compute_programs)
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

void Device::bind_global_uniform_buffer(Handle<Buffer> buffer_handle, usize offset, usize range)
{
    global_set.pending_buffer = buffer_handle;
    global_set.pending_offset = offset;
    global_set.pending_range = range;
}

void Device::bind_global_storage_image(u32 index, Handle<Image> image_handle)
{
    global_set.storage_images[index] = image_handle;

    global_set.pending_images.push_back(image_handle);
    global_set.pending_indices.push_back(index);
    global_set.pending_binding.push_back(2);

    global_set.current_storage_image += 1;
}

void Device::bind_global_sampled_image(u32 index, Handle<Image> image_handle)
{
    if (global_set.sampled_images[index] == image_handle) {
        return;
    }

    global_set.sampled_images[index] = image_handle;

    global_set.pending_images.push_back(image_handle);
    global_set.pending_indices.push_back(index);
    global_set.pending_binding.push_back(1);

    global_set.current_sampled_image += 1;
}

void Device::update_globals()
{
    Vec<VkWriteDescriptorSet> writes;
    writes.reserve(global_set.pending_images.size() + 1);

    // writes' elements contain pointers to these buffers, so they have to be allocated with the right size
    Vec<VkDescriptorImageInfo> images_info;
    images_info.reserve(global_set.pending_images.size());

    for (uint i_pending = 0; i_pending < global_set.pending_images.size(); i_pending++)
    {
        writes.emplace_back();
        auto &write = writes.back();
        write                  = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet           = global_set.vkset;
        write.dstBinding       = global_set.pending_binding[i_pending];
        write.dstArrayElement  = global_set.pending_indices[i_pending];
        write.descriptorCount  = 1;
        write.descriptorType   = writes[i_pending].dstBinding == 1 ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

        auto &image = *images.get(global_set.pending_images[i_pending]);
        images_info.push_back({
                .sampler = samplers[BuiltinSampler::Default],
                .imageView = image.full_view.vkhandle,
                .imageLayout = writes[i_pending].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL,
            });
        write.pImageInfo = &images_info.back();
    }

    VkDescriptorBufferInfo buffer_info;
    buffer_info.range  = global_set.pending_range;
    buffer_info.offset = 0;

    if (auto *pending_buffer = buffers.get(global_set.pending_buffer))
    {
        buffer_info.buffer = pending_buffer->vkhandle;
    }

    if (global_set.dynamic_offset != global_set.pending_offset)
    {
        global_set.dynamic_offset = global_set.pending_offset;
    }

    if (global_set.dynamic_buffer != global_set.pending_buffer || global_set.dynamic_range != global_set.pending_range)
    {
        global_set.dynamic_buffer = global_set.pending_buffer;
        global_set.dynamic_range  = global_set.pending_range;

        writes.push_back({
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = global_set.vkset,
            .dstBinding      = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .pBufferInfo     = &buffer_info,
        });
    }

    vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
    global_set.pending_images.clear();
    global_set.pending_indices.clear();
    global_set.pending_binding.clear();
}

}
