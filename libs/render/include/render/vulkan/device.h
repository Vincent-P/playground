#pragma once
#include <exo/collections/pool.h>

#include "render/vulkan/commands.h"
#include "render/vulkan/descriptor_set.h"
#include "render/vulkan/physical_device.h"
#include "render/vulkan/synchronization.h"

#include "exo/string_view.h"
#include <volk.h>

VK_DEFINE_HANDLE(VmaAllocator);
namespace vulkan
{
struct Surface;
struct Context;
struct WorkPool;
struct QueryPool;
struct Shader;
struct GraphicsProgram;
struct GraphicsState;
struct RenderState;
struct ComputeProgram;
struct ComputeState;
struct RenderPass;
struct Framebuffer;
struct FramebufferFormat;
struct ImageDescription;
struct BufferDescription;

enum BuiltinSampler
{
	Default = 0,
	Nearest = 1,
	Count
};

struct GlobalDescriptorSets
{
	VkDescriptorPool             uniform_descriptor_pool = VK_NULL_HANDLE;
	VkDescriptorSetLayout        uniform_layout          = VK_NULL_HANDLE;
	BindlessSet                  bindless                = {};
	VkPipelineLayout             pipeline_layout         = VK_NULL_HANDLE;
	Vec<DynamicBufferDescriptor> uniform_descriptors     = {};
};

struct PushConstantLayout
{
	usize size = 0;
};

struct DeviceDescription
{
	const PhysicalDevice *physical_device       = nullptr;
	PushConstantLayout    push_constant_layout  = {};
	bool                  buffer_device_address = false;
};

struct Device
{
	DeviceDescription desc                = {};
	VkDevice          device              = VK_NULL_HANDLE;
	PhysicalDevice    physical_device     = {};
	u32               graphics_family_idx = u32_invalid;
	u32               compute_family_idx  = u32_invalid;
	u32               transfer_family_idx = u32_invalid;
	VmaAllocator      allocator           = VK_NULL_HANDLE;

	PushConstantLayout   push_constant_layout;
	GlobalDescriptorSets global_sets;

	exo::Pool<Shader>          shaders;
	exo::Pool<GraphicsProgram> graphics_programs;
	exo::Pool<ComputeProgram>  compute_programs;
	exo::Pool<Framebuffer>     framebuffers;
	exo::Pool<Image>           images;
	exo::Pool<Buffer>          buffers;
	Vec<VkSampler>             samplers;

	/// ---

	static Device create(const Context &context, const DeviceDescription &desc);
	void          destroy(const Context &context);

	/// --- Resources
	// Command submission
	void         create_work_pool(WorkPool &work_pool);
	void         reset_work_pool(WorkPool &work_pool);
	void         destroy_work_pool(WorkPool &work_pool);
	GraphicsWork get_graphics_work(WorkPool &work_pool);
	ComputeWork  get_compute_work(WorkPool &work_pool);
	TransferWork get_transfer_work(WorkPool &work_pool);

	void         create_query_pool(QueryPool &query_pool, u32 query_capacity);
	void         reset_query_pool(QueryPool &query_pool, u32 first_query, u32 count);
	void         destroy_query_pool(QueryPool &query_pool);
	void         get_query_results(QueryPool &query_pool, u32 first_query, u32 count, Vec<u64> &results);
	inline float get_ns_per_timestamp() const { return physical_device.properties.limits.timestampPeriod; }

	Fence create_fence(u64 initial_value = 0);
	u64   get_fence_value(Fence &fence);
	void  set_fence_value(Fence &fence, u64 value);
	void  destroy_fence(Fence &fence);

	void wait_for_fence(const Fence &fence, u64 wait_value);
	void wait_for_fences(exo::Span<const Fence> fences, exo::Span<const u64> wait_values);
	void wait_idle();
	void submit(Work &work, exo::Span<const Fence> signal_fences, exo::Span<const u64> signal_values);

	// Shaders
	Handle<Shader> create_shader(exo::StringView path);
	void           reload_shader(Handle<Shader> shader_handle);
	void           destroy_shader(Handle<Shader> shader_handle);

	// Graphics Pipeline
	Handle<GraphicsProgram> create_program(exo::StringView name, const GraphicsState &graphics_state);
	void                    destroy_program(Handle<GraphicsProgram> program_handle);
	u32  compile_graphics_state(Handle<GraphicsProgram> &program_handle, const RenderState &render_state);
	void compile_graphics_pipeline(Handle<GraphicsProgram> &program_handle, usize i_pipeline);

	// Framebuffers
	Handle<Framebuffer> create_framebuffer(
		int3 size, exo::Span<const Handle<Image>> color_attachments, Handle<Image> depth_attachment = {});
	void destroy_framebuffer(Handle<Framebuffer> framebuffer_handle);

	RenderPass &find_or_create_renderpass(Framebuffer &framebuffer, exo::Span<const LoadOp> load_ops); // private

	// Compute pipeline
	void                   recreate_program_internal(ComputeProgram &compute_program);
	Handle<ComputeProgram> create_program(exo::StringView name, const ComputeState &compute_state);
	void                   destroy_program(Handle<ComputeProgram> program_handle);

	// Resources
	Handle<Image> create_image(const ImageDescription &image_desc, Option<VkImage> proxy = {});
	void          destroy_image(Handle<Image> image_handle);
	u32           get_image_sampled_index(Handle<Image> image_handle) const;
	u32           get_image_storage_index(Handle<Image> image_handle) const;
	int3          get_image_size(Handle<Image> image_handle);
	void          unbind_image(Handle<Image> image_handle);

	Handle<Buffer> create_buffer(const BufferDescription &buffer_desc);
	void           destroy_buffer(Handle<Buffer> buffer_handle);
	u32            get_buffer_storage_index(Handle<Buffer> buffer_handle);

	void                           *map_buffer(Handle<Buffer> buffer_handle);
	u64                             get_buffer_address(Handle<Buffer> buffer_handle);
	usize                           get_buffer_size(Handle<Buffer> buffer_handle);
	template <typename T> inline T *map_buffer(Handle<Buffer> buffer_handle)
	{
		return reinterpret_cast<T *>(map_buffer(buffer_handle));
	}
	void flush_buffer(Handle<Buffer> buffer_handle);

	// Global descriptor set
	void                           update_globals();
	const DynamicBufferDescriptor &find_or_create_uniform_descriptor(Handle<Buffer> buffer_handle, usize size);

	// Swapchain
	bool acquire_next_swapchain(Surface &surface);
	bool present(Surface &surface, Work &work);
};

} // namespace vulkan
