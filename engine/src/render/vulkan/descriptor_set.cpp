#include "render/vulkan/descriptor_set.hpp"

#include "render/vulkan/device.hpp"
#include "render/vulkan/utils.hpp"
#include "vulkan/vulkan_core.h"

#include <algorithm>
#include <ranges>

namespace vulkan
{
DescriptorSet create_descriptor_set(Device &device, const Vec<DescriptorType> &descriptors)
{
    DescriptorSet descriptor_set = {};

    Vec<VkDescriptorSetLayoutBinding> bindings;

    uint binding_number = 0;
    for (usize i_descriptor = 0; i_descriptor < descriptors.size(); i_descriptor++)
    {
        auto &descriptor_type = descriptors[i_descriptor];

        bindings.emplace_back();
        auto &binding = bindings.back();
        binding.binding = binding_number;
        binding.descriptorType = to_vk(descriptor_type);
        binding.descriptorCount = descriptor_type.count ? descriptor_type.count : 1;
        binding.stageFlags = VK_SHADER_STAGE_ALL;
        binding_number += 1;

        if (descriptor_type.type == DescriptorType::DynamicBuffer)
        {
            descriptor_set.dynamic_descriptors.push_back(i_descriptor);
        }
    }

    descriptor_set.dynamic_offsets.resize(descriptor_set.dynamic_descriptors.size());

    Descriptor empty = {{{}}};
    descriptor_set.descriptors.resize(descriptors.size(), empty);

    VkDescriptorSetLayoutCreateInfo desc_layout_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    desc_layout_info.bindingCount = static_cast<u32>(bindings.size());
    desc_layout_info.pBindings    = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(device.device, &desc_layout_info, nullptr, &descriptor_set.layout));

    descriptor_set.descriptor_desc = descriptors;

    return descriptor_set;
}

void destroy_descriptor_set(Device &device, DescriptorSet &set)
{
    if (!set.vkhandles.empty())
    {
        vkFreeDescriptorSets(device.device, device.descriptor_pool, static_cast<u32>(set.vkhandles.size()), set.vkhandles.data());
    }
    vkDestroyDescriptorSetLayout(device.device, set.layout, nullptr);
}

void bind_image(DescriptorSet &set, u32 slot, Handle<Image> image_handle)
{
    assert(set.descriptor_desc[slot].type == DescriptorType::SampledImage
           || set.descriptor_desc[slot].type == DescriptorType::StorageImage);
    set.descriptors[slot].image = {image_handle};
}


void bind_uniform_buffer(DescriptorSet &set, u32 slot, Handle<Buffer> buffer_handle, u32 offset, usize size)
{
    assert(set.descriptor_desc[slot].type == DescriptorType::DynamicBuffer);
    set.descriptors[slot].dynamic = {buffer_handle, size, offset};

    for (usize i_dynamic = 0; i_dynamic < set.dynamic_descriptors.size(); i_dynamic++)
    {
        auto dynamic_descriptor_idx = set.dynamic_descriptors[i_dynamic];
        if (slot == dynamic_descriptor_idx)
        {
            set.dynamic_offsets[i_dynamic] = offset;
            return;
        }
    }

    logger::error("Descriptor #{} is not a dynamic buffer.\n", slot);
}

void bind_storage_buffer(DescriptorSet &set, u32 slot, Handle<Buffer> buffer_handle)
{
    assert(set.descriptor_desc[slot].type == DescriptorType::StorageBuffer);
    set.descriptors[slot].buffer = {buffer_handle};
}

VkDescriptorSet find_or_create_descriptor_set(Device &device, DescriptorSet &set)
{
    auto hash = hash_value(set.descriptors);

    for (usize i = 0; i < set.hashes.size(); i++)
    {
        if (hash == set.hashes[i])
        {
            return set.vkhandles[i];
        }
    }

    set.hashes.push_back(hash);

    VkDescriptorSetAllocateInfo set_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    set_info.descriptorPool              = device.descriptor_pool;
    set_info.pSetLayouts                 = &set.layout;
    set_info.descriptorSetCount          = 1;

    VkDescriptorSet vkhandle = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateDescriptorSets(device.device, &set_info, &vkhandle));

    Vec<VkWriteDescriptorSet> writes(set.descriptors.size());

    // writes' elements contain pointers to these buffers, so they have to be allocated with the right size
    Vec<VkDescriptorImageInfo> images_info;
    Vec<VkDescriptorBufferInfo> buffers_info;
    buffers_info.reserve(set.descriptors.size());
    images_info.reserve(set.descriptors.size());

    for (uint slot = 0; slot < set.descriptors.size(); slot++)
    {
        writes[slot]                  = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[slot].dstSet           = vkhandle;
        writes[slot].dstBinding       = slot;
        writes[slot].descriptorCount  = set.descriptor_desc[slot].count;
        writes[slot].descriptorType   = to_vk(set.descriptor_desc[slot]);

        if (set.descriptor_desc[slot].type == DescriptorType::SampledImage)
        {
            if (!set.descriptors[slot].image.image_handle.is_valid())
                logger::error("Binding #{} has an invalid image handle.\n", slot);

            auto &image = *device.images.get(set.descriptors[slot].image.image_handle);
            images_info.push_back({
                    .sampler = device.samplers[BuiltinSampler::Default],
                    .imageView = image.full_view.vkhandle,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                });
            writes[slot].pImageInfo = &images_info.back();
        }
        else if (set.descriptor_desc[slot].type == DescriptorType::StorageImage)
        {
            if (!set.descriptors[slot].image.image_handle.is_valid())
                logger::error("Binding #{} has an invalid image handle.\n", slot);

            auto &image = *device.images.get(set.descriptors[slot].image.image_handle);
            images_info.push_back({
                    .sampler = VK_NULL_HANDLE,
                    .imageView = image.full_view.vkhandle,
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                });
            writes[slot].pImageInfo = &images_info.back();
        }
        else if (set.descriptor_desc[slot].type == DescriptorType::DynamicBuffer)
        {
            DynamicDescriptor &dynamic_descriptor = set.descriptors[slot].dynamic;
            if (!dynamic_descriptor.buffer_handle.is_valid())
                logger::error("Binding #{} has an invalid buffer handle.\n", slot);

            auto &buffer = *device.buffers.get(dynamic_descriptor.buffer_handle);
            buffers_info.push_back({
                    .buffer = buffer.vkhandle,
                    .offset = 0,
                    .range = dynamic_descriptor.size,
                });
            writes[slot].pBufferInfo = &buffers_info.back();
        }
        else if (set.descriptor_desc[slot].type == DescriptorType::StorageBuffer)
        {
            BufferDescriptor &buffer_descriptor = set.descriptors[slot].buffer;
            if (!buffer_descriptor.buffer_handle.is_valid())
                logger::error("Binding #{} has an invalid buffer handle.\n", slot);

            auto &buffer = *device.buffers.get(buffer_descriptor.buffer_handle);
            buffers_info.push_back({
                    .buffer = buffer.vkhandle,
                    .offset = 0,
                    .range = buffer.desc.size,
                });
            writes[slot].pBufferInfo = &buffers_info.back();
        }
        else
        {
            logger::error("Binding #{} has an invalid descriptor type.\n", slot);
        }
    }

    vkUpdateDescriptorSets(device.device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);

    set.vkhandles.push_back(vkhandle);

    return vkhandle;
}

};
