#include "render/vulkan/bindless_set.h"

#include "render/vulkan/device.h"
#include "render/vulkan/descriptor_set.h"
#include "render/vulkan/utils.h"

namespace vulkan
{
BindlessSet create_bindless_set(const Device &device, VkDescriptorPool pool, const char *name, DescriptorType type)
{
    BindlessSet set = {};
    set.descriptor_type = type;

    VkDescriptorSetLayoutBinding binding = {.binding         = 0,
                                            .descriptorType  = to_vk(set.descriptor_type),
                                            .descriptorCount = type.count,
                                            .stageFlags      = VK_SHADER_STAGE_ALL};

    VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

    VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
    flags_info.bindingCount  = 1;
    flags_info.pBindingFlags = &flags;

    VkDescriptorSetLayoutCreateInfo desc_layout_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    desc_layout_info.pNext                           = &flags_info;
    desc_layout_info.flags                           = 0;
    desc_layout_info.bindingCount                    = 1;
    desc_layout_info.pBindings                       = &binding;

    VK_CHECK(vkCreateDescriptorSetLayout(device.device, &desc_layout_info, nullptr, &set.layout));

    VkDescriptorSetAllocateInfo set_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    set_info.descriptorPool              = pool;
    set_info.pSetLayouts                 = &set.layout;
    set_info.descriptorSetCount          = 1;
    VK_CHECK(vkAllocateDescriptorSets(device.device, &set_info, &set.set));


    if (device.vkSetDebugUtilsObjectNameEXT)
    {
        VkDebugUtilsObjectNameInfoEXT ni = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        ni.objectHandle                  = reinterpret_cast<u64>(set.set);
        ni.objectType                    = VK_OBJECT_TYPE_DESCRIPTOR_SET;
        ni.pObjectName                   = name;
        VK_CHECK(device.vkSetDebugUtilsObjectNameEXT(device.device, &ni));
    }

    set.free_list = exo::FreeList::create(type.count);
    set.descriptors.resize(type.count, {.dynamic = {}});

    return set;
}

void destroy_bindless_set(const Device &device, BindlessSet &set)
{
    vkDestroyDescriptorSetLayout(device.device, set.layout, nullptr);
    set.free_list.destroy();
    set = {};
}

u32 bind_descriptor(BindlessSet &set, Descriptor desc)
{
    u32 new_index = set.free_list.allocate();
    set.descriptors[new_index] = desc;
    set.pending_bind.push_back(new_index);
    return new_index;
}

void unbind_descriptor(BindlessSet &set, u32 index)
{
    set.descriptors[index].dynamic = {};
    set.free_list.free(index);
    set.pending_unbind.push_back(index);
}

void update_bindless_set(Device &device, BindlessSet &set)
{
    if (set.pending_bind.empty() && set.pending_unbind.empty())
    {
        return;
    }

    Vec<VkWriteDescriptorSet> writes;
    writes.reserve(set.pending_bind.size());

    Vec<VkCopyDescriptorSet> copies;
    copies.reserve(set.pending_unbind.size());

    // writes' elements contain pointers to these buffers, so they have to be allocated with the right size
    Vec<VkDescriptorImageInfo> images_info;
    Vec<VkDescriptorBufferInfo> buffers_info;
    if (set.descriptor_type.type == DescriptorType::SampledImage || set.descriptor_type.type == DescriptorType::StorageImage) {
        images_info.reserve(writes.capacity());
    }
    else if (set.descriptor_type.type == DescriptorType::StorageBuffer) {
        buffers_info.reserve(writes.capacity());
    }

    VkDescriptorType desc_type = to_vk(set.descriptor_type);
    VkImageLayout img_layout   = desc_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
                                     ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                     : VK_IMAGE_LAYOUT_GENERAL;

    for (auto to_bind : set.pending_bind)
    {
        writes.emplace_back();
        auto &write = writes.back();
        write                  = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet           = set.set;
        write.dstBinding       = 0;
        write.dstArrayElement  = to_bind;
        write.descriptorCount  = 1;
        write.descriptorType   = desc_type;

        if (set.descriptor_type.type == DescriptorType::StorageImage || set.descriptor_type.type == DescriptorType::SampledImage)
        {
            const auto &descriptor = set.descriptors[to_bind].image;
            const auto &image = *device.images.get(descriptor.image_handle);
            images_info.push_back({
                .sampler     = device.samplers[BuiltinSampler::Default],
                .imageView   = image.full_view.vkhandle,
                .imageLayout = img_layout,
            });
            write.pImageInfo = &images_info.back();
        }
        else if (set.descriptor_type.type == DescriptorType::StorageBuffer)
        {
            const auto &descriptor = set.descriptors[to_bind].buffer;
            const auto &buffer = *device.buffers.get(descriptor.buffer_handle);

            buffers_info.push_back({
                .buffer = buffer.vkhandle,
                .offset = 0,
                .range  = buffer.desc.size,
            });
            write.pBufferInfo = &buffers_info.back();
        }
        else
        {
            ASSERT(false);
        }
    }

    // Copy descriptor #0 (null, empty image) to unbind
    for (auto to_unbind : set.pending_unbind)
    {
        bool already_bound = false;
        for (auto to_bind : set.pending_bind)
        {
            if (to_unbind == to_bind)
            {
                already_bound = true;
                break;
            }
        }
        if (already_bound) { continue; }

        copies.emplace_back();
        auto &copy = copies.back();
        copy                  = {.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET};
        copy.srcSet           = set.set;
        copy.srcBinding       = 0;
        copy.srcArrayElement  = 0;
        copy.dstSet           = set.set;
        copy.dstBinding       = 0;
        copy.dstArrayElement  = to_unbind;
        copy.descriptorCount  = 1;
    }

    vkUpdateDescriptorSets(device.device, static_cast<u32>(writes.size()), writes.data(), static_cast<u32>(copies.size()), copies.data());
    set.pending_bind.clear();
    set.pending_unbind.clear();
}

Handle<Image> get_image_descriptor(BindlessSet &set, u32 index)
{
    return set.descriptors[index].image.image_handle;
}

}
