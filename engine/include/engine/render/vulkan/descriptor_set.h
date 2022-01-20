#pragma once
#include <exo/hash.h>
#include <exo/collections/handle.h>
#include <exo/collections/vector.h>

#include <volk.h>
#include <span>

namespace vulkan
{
struct Device;
struct Image;
struct Buffer;
struct GraphicsState;

struct ImageDescriptor
{
    Handle<Image> image_handle;
};

struct BufferDescriptor
{
    Handle<Buffer> buffer_handle;
};

struct DynamicDescriptor
{
    Handle<Buffer> buffer_handle;
    usize          size;
    usize          offset;
};

union DescriptorType
{
    static const u32 Empty         = 0;
    static const u32 SampledImage  = 1;
    static const u32 StorageImage  = 2;
    static const u32 StorageBuffer = 3;
    static const u32 DynamicBuffer = 4;

    struct
    {
        u32 count : 24;
        u32 type : 8;
    } bits;
    u32 raw;
};

union Descriptor
{
    ImageDescriptor   image;
    BufferDescriptor  buffer;
    DynamicDescriptor dynamic;

    // for std::hash
    struct
    {
        u64 one;
        u64 two;
        u64 three;
    } raw;
};

struct DescriptorSet
{
    VkDescriptorSetLayout layout;
    Vec<Descriptor>       descriptors;
    Vec<DescriptorType>   descriptor_desc;

    // linear map
    Vec<VkDescriptorSet> vkhandles;
    Vec<usize>           hashes;

    // dynamic offsets
    Vec<usize> dynamic_descriptors;
    Vec<usize> dynamic_offsets;
};

DescriptorSet create_descriptor_set(Device &device, std::span<const DescriptorType> descriptors);
void          destroy_descriptor_set(Device &device, DescriptorSet &set);

void bind_uniform_buffer(DescriptorSet &set, u32 slot, Handle<Buffer> buffer_handle, usize offset, usize size);
void bind_storage_buffer(DescriptorSet &set, u32 slot, Handle<Buffer> buffer_handle);
void bind_image(DescriptorSet &set, u32 slot, Handle<Image> image_handle);

VkDescriptorSet find_or_create_descriptor_set(Device &device, DescriptorSet &set);

// -- Utils

inline VkDescriptorType to_vk(DescriptorType desc_type)
{
    switch (desc_type.bits.type)
    {
    case DescriptorType::SampledImage:
        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case DescriptorType::StorageImage:
        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case DescriptorType::StorageBuffer:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case DescriptorType::DynamicBuffer:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    }
    ASSERT(false);
    return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}

} // namespace vulkan

namespace std
{
template <>
struct hash<vulkan::Descriptor>
{
    std::size_t operator()(vulkan::Descriptor const &descriptor) const noexcept
    {
        usize hash = descriptor.raw.one;
        return hash;
    }
};
} // namespace std
