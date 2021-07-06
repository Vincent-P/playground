#pragma once
#include "render/vulkan/descriptor_set.hpp"
#include "base/free_list.hpp"
#include "base/handle.hpp"

namespace vulkan
{
struct Image;

struct BindlessSet
{
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    DescriptorType descriptor_type = {};
    Vec<Descriptor> descriptors = {};
    FreeList free_list = {};

    Vec<u32> pending_bind = {};
    Vec<u32> pending_unbind = {};
};

BindlessSet create_bindless_set(const Device &device, VkDescriptorPool pool, const char *name, DescriptorType type);
void destroy_bindless_set(const Device &device, BindlessSet &set);
u32 bind_descriptor(BindlessSet &set, Descriptor desc);
void unbind_descriptor(BindlessSet &set, u32 index);
void update_bindless_set(Device &device, BindlessSet &set);

Handle<Image> get_image_descriptor(BindlessSet &set, u32 index);
}
