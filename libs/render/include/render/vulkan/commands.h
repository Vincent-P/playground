#pragma once
#include <exo/collections/dynamic_array.h>
#include <exo/collections/enum_array.h>
#include <exo/collections/handle.h>
#include <exo/collections/vector.h>
#include <exo/maths/vectors.h>
#include <exo/option.h>

#include "render/vulkan/queues.h"
#include "render/vulkan/synchronization.h"

#include "exo/string_view.h"
#include <volk.h>

namespace vulkan
{
struct Device;
struct Image;
struct Buffer;
struct Surface;
struct QueryPool;
struct ComputeProgram;
struct GraphicsProgram;
struct Framebuffer;
struct LoadOp;
struct DynamicBufferDescriptor;
enum struct BufferUsage : u8;
enum struct ImageUsage : u8;

inline constexpr usize MAX_SEMAPHORES = 4; // Maximum number of waitable semaphores per command buffer

struct CommandPool
{
	VkCommandPool        vk_handle               = VK_NULL_HANDLE;
	Vec<VkCommandBuffer> command_buffers         = {};
	Vec<u32>             command_buffers_is_used = {};
};

struct WorkPool
{
	exo::EnumArray<CommandPool, QueueType> command_pools;

	inline CommandPool &graphics() { return command_pools[QueueType::Graphics]; }
	inline CommandPool &compute() { return command_pools[QueueType::Compute]; }
	inline CommandPool &transfer() { return command_pools[QueueType::Transfer]; }
};

// Command buffer / Queue abstraction
struct Work
{
	Device *device;

	VkCommandBuffer                                         command_buffer;
	exo::DynamicArray<Fence, MAX_SEMAPHORES>                wait_fence_list;
	exo::DynamicArray<u64, MAX_SEMAPHORES>                  wait_value_list;
	exo::DynamicArray<VkPipelineStageFlags, MAX_SEMAPHORES> wait_stage_list;
	VkQueue                                                 queue;
	QueueType                                               queue_type;

	// vulkan hacks:
	Option<VkSemaphore>          image_acquired_semaphore;
	Option<VkPipelineStageFlags> image_acquired_stage;
	Option<VkSemaphore>          signal_present_semaphore;

	void begin();
	void bind_global_set();
	void bind_uniform_set(const DynamicBufferDescriptor &dynamic_descriptor, u32 offset, u32 i_set = 2);
	void end();

	void wait_for(Fence &fence, u64 wait_value, VkPipelineStageFlags stage_dst);

	// vulkan hacks:
	void wait_for_acquired(Surface &surface, VkPipelineStageFlags stage_dst);
	void prepare_present(Surface &surface);

	void barrier(Handle<Buffer> buffer, BufferUsage usage_destination);
	void absolute_barrier(Handle<Image> image_handle);
	void barrier(Handle<Image> image, ImageUsage usage_destination);
	void clear_barrier(Handle<Image> image, ImageUsage usage_destination);
	void barriers(exo::Span<std::pair<Handle<Image>, ImageUsage>> images,
		exo::Span<std::pair<Handle<Buffer>, BufferUsage>>         buffers);

	// queries
	void reset_query_pool(QueryPool &query_pool, u32 first_query, u32 count);
	void begin_query(QueryPool &query_pool, u32 index);
	void end_query(QueryPool &query_pool, u32 index);
	void timestamp_query(QueryPool &query_pool, u32 index);

	// debug utils
	void begin_debug_label(exo::StringView label, float4 color = float4(0.0f));
	void end_debug_label();
};

struct TransferWork : Work
{
	void copy_buffer(
		Handle<Buffer> src, Handle<Buffer> dst, exo::Span<const std::tuple<usize, usize, usize>> offsets_src_dst_size);
	void copy_buffer(Handle<Buffer> src, Handle<Buffer> dst);
	void copy_image(Handle<Image> src, Handle<Image> dst);
	void blit_image(Handle<Image> src, Handle<Image> dst);
	void copy_buffer_to_image(Handle<Buffer> src, Handle<Image> dst, exo::Span<VkBufferImageCopy> regions);
	void fill_buffer(Handle<Buffer> buffer_handle, u32 data);
	void transfer();
};

struct ComputeWork : TransferWork
{
	void bind_pipeline(Handle<ComputeProgram> program_handle);
	void dispatch(uint3 workgroups);

	void clear_image(Handle<Image> image, VkClearColorValue clear_color);

	void                       push_constant(const void *data, usize len);
	template <typename T> void push_constant(const T &object) { this->push_constant(&object, sizeof(T)); }
};

struct DrawIndexedOptions
{
	u32 vertex_count    = 0;
	u32 instance_count  = 1;
	u32 index_offset    = 0;
	i32 vertex_offset   = 0;
	u32 instance_offset = 0;
};

struct DrawOptions
{
	u32 vertex_count    = 0;
	u32 instance_count  = 1;
	u32 vertex_offset   = 0;
	u32 instance_offset = 0;
};

struct DrawIndexedIndirectCountOptions
{
	Handle<Buffer> arguments_buffer = {};
	usize          arguments_offset = 0;
	Handle<Buffer> count_buffer     = {};
	usize          count_offset     = 0;
	u32            max_draw_count   = 0;
	u32            stride           = sizeof(DrawIndexedOptions);
};

struct GraphicsWork : ComputeWork
{
	void draw_indexed(const DrawIndexedOptions &options);
	void draw(const DrawOptions &options);
	void draw_indexed_indirect_count(const DrawIndexedIndirectCountOptions &options);

	void set_scissor(const VkRect2D &rect);
	void set_viewport(const VkViewport &viewport);

	void begin_pass(Handle<Framebuffer> framebuffer_handle, exo::Span<const LoadOp> load_ops);
	void end_pass();

	using ComputeWork::bind_pipeline; // make it visible on GraphicsWork
	void bind_pipeline(Handle<GraphicsProgram> program_handle, uint pipeline_index);
	void bind_index_buffer(
		Handle<Buffer> buffer_handle, VkIndexType index_type = VK_INDEX_TYPE_UINT16, usize offset = 0);
};
} // namespace vulkan
