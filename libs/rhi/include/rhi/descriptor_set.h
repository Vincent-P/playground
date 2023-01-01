#pragma once
#include "exo/collections/handle.h"
#include "exo/collections/vector.h"

#include <volk.h>

namespace rhi
{
struct Buffer;
struct Image;
struct Device;

struct DynamicBufferDescriptor
{
	Handle<Buffer>  buffer;
	VkDescriptorSet vkset;
	u32             size;
};

DynamicBufferDescriptor create_buffer_descriptor(Device &device, Handle<Buffer> &buffer_handle, usize range_size);
void                    destroy_buffer_descriptor(Device &device, DynamicBufferDescriptor &descriptor);

struct BindlessSet
{
	template <typename T> using PerSet = T[3];
	static constexpr usize PER_SAMPLER = 0;
	static constexpr usize PER_IMAGE   = 1;
	static constexpr usize PER_BUFFER  = 2;

	VkDescriptorPool      vkpool   = VK_NULL_HANDLE;
	VkDescriptorSetLayout vklayout = VK_NULL_HANDLE;
	VkDescriptorSet       vkset    = VK_NULL_HANDLE;

	Vec<Handle<Image>>  sampler_images  = {};
	Vec<Handle<Image>>  storage_images  = {};
	Vec<Handle<Buffer>> storage_buffers = {};
	PerSet<Vec<u32>>    free_list       = {};
	PerSet<Vec<u32>>    pending_bind    = {};
	PerSet<Vec<u32>>    pending_unbind  = {};
};

BindlessSet create_bindless_set(const Device &device, u32 sampler_count, u32 image_count, u32 buffer_count);
void        destroy_bindless_set(const Device &device, BindlessSet &set);

u32 bind_sampler_image(BindlessSet &bindless, Handle<Image> image_handle);
u32 bind_storage_image(BindlessSet &bindless, Handle<Image> image_handle);
u32 bind_storage_buffer(BindlessSet &bindless, Handle<Buffer> buffer_handle);

void unbind_sampler_image(BindlessSet &set, u32 index);
void unbind_storage_image(BindlessSet &set, u32 index);
void unbind_storage_buffer(BindlessSet &set, u32 index);

void update_bindless_set(Device &device, BindlessSet &set);

Handle<Image>  get_sampler_image_at(BindlessSet &set, u32 index);
Handle<Image>  get_storage_image_at(BindlessSet &set, u32 index);
Handle<Buffer> get_storage_buffer_at(BindlessSet &set, u32 index);

} // namespace rhi
