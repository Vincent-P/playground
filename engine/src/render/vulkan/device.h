#pragma once
#include <exo/types.h>
#include <exo/option.h>
#include <exo/collections/vector.h>
#include <exo/collections/pool.h>

#include "render/vulkan/bindless_set.h"
#include "render/vulkan/buffer.h" // needed for Pool<Image> TODO: investigate why we cant fwd declare
#include "render/vulkan/commands.h"
#include "render/vulkan/descriptor_set.h"
#include "render/vulkan/image.h" // needed for Pool<Image> TODO: investigate why we cant fwd declare
#include "render/vulkan/physical_device.h"
#include "render/vulkan/pipelines.h"
#include "render/vulkan/synchronization.h"

#include <vk_mem_alloc.h>

namespace vulkan
{
struct Surface;
struct Context;
struct WorkPool;
struct QueryPool;

enum BuiltinSampler
{
    Default = 0,
    Nearest = 1,
    Count
};

struct GlobalDescriptorSets
{
    VkDescriptorPool pool;
    VkPipelineLayout pipeline_layout;
    DescriptorSet uniform;
    BindlessSet storage_images;
    BindlessSet sampled_images;
    BindlessSet storage_buffers;
};

struct PushConstantLayout
{
    usize size;
};

struct DeviceDescription
{
    const PhysicalDevice *physical_device;
    PushConstantLayout push_constant_layout;
    bool buffer_device_address;
};

struct Device
{
    DeviceDescription desc;
    VkDevice device = VK_NULL_HANDLE;
    PhysicalDevice physical_device;
    u32 graphics_family_idx = u32_invalid;
    u32 compute_family_idx = u32_invalid;
    u32 transfer_family_idx = u32_invalid;
    VmaAllocator allocator;

    VkDescriptorPool descriptor_pool;
    PushConstantLayout push_constant_layout;
    GlobalDescriptorSets global_sets;

    Pool<Shader> shaders;
    Pool<GraphicsProgram> graphics_programs;
    Pool<ComputeProgram> compute_programs;
    Pool<Framebuffer> framebuffers;
    Pool<Image> images;
    Pool<Buffer> buffers;
    Vec<VkSampler> samplers;

    /// ---

    static Device create(const Context &context, const DeviceDescription &desc);
    void destroy(const Context &context);

#define X(name) PFN_##name name
    X(vkCreateDebugUtilsMessengerEXT);
    X(vkDestroyDebugUtilsMessengerEXT);
    X(vkCmdBeginDebugUtilsLabelEXT);
    X(vkCmdEndDebugUtilsLabelEXT);
    X(vkSetDebugUtilsObjectNameEXT);
#undef X

    /// --- Resources
    // Command submission
    void create_work_pool(WorkPool &work_pool);
    void reset_work_pool(WorkPool &work_pool);
    void destroy_work_pool(WorkPool &work_pool);
    GraphicsWork get_graphics_work(WorkPool &work_pool);
    ComputeWork  get_compute_work (WorkPool &work_pool);
    TransferWork get_transfer_work(WorkPool &work_pool);

    void create_query_pool(QueryPool &query_pool, u32 query_capacity);
    void reset_query_pool(QueryPool &query_pool, u32 first_query, u32 count);
    void destroy_query_pool(QueryPool &query_pool);
    void get_query_results(QueryPool &query_pool, u32 first_query, u32 count, Vec<u64> &results);
    inline float get_ns_per_timestamp() const { return physical_device.properties.limits.timestampPeriod; }

    Fence create_fence(u64 initial_value = 0);
    u64 get_fence_value(Fence &fence);
    void set_fence_value(Fence &fence, u64 value);
    void destroy_fence(Fence &fence);

    void wait_for_fence(const Fence &fence, u64 wait_value);
    void wait_for_fences(const Vec<Fence> &fences, const Vec<u64> &wait_values);
    void wait_idle();
    void submit(Work &work, const Vec<Fence> &signal_fences, const Vec<u64> &signal_values);

    // Shaders
    Handle<Shader> create_shader(std::string_view path);
    void destroy_shader(Handle<Shader> shader_handle);

    // Graphics Pipeline
    Handle<GraphicsProgram> create_program(std::string name, const GraphicsState &graphics_state);
    void destroy_program(Handle<GraphicsProgram> program_handle);
    unsigned compile(Handle<GraphicsProgram> &program_handle, const RenderState &render_state);

    // Framebuffers
    Handle<Framebuffer> create_framebuffer(const FramebufferFormat &fb_desc, const Vec<Handle<Image>> &color_attachments, Handle<Image> depth_attachment = {});
    void destroy_framebuffer(Handle<Framebuffer> framebuffer_handle);

    RenderPass &find_or_create_renderpass(Framebuffer &framebuffer, const Vec<LoadOp> &load_ops); // private

    // Compute pipeline
    void recreate_program_internal(ComputeProgram &compute_program);
    Handle<ComputeProgram> create_program(std::string name, const ComputeState &compute_state);
    void destroy_program(Handle<ComputeProgram> program_handle);

    // Resources
    Handle<Image> create_image(const ImageDescription &image_desc, Option<VkImage> proxy = {});
    void destroy_image(Handle<Image> image_handle);
    u32 get_image_sampled_index(Handle<Image> image_handle);
    u32 get_image_storage_index(Handle<Image> image_handle);
    int3 get_image_size(Handle<Image> image_handle);

    Handle<Buffer> create_buffer(const BufferDescription &buffer_desc);
    void destroy_buffer(Handle<Buffer> buffer_handle);
    u32 get_buffer_storage_index(Handle<Buffer> buffer_handle);

    void *map_buffer(Handle<Buffer> buffer_handle);
    u64 get_buffer_address(Handle<Buffer> buffer_handle);
    usize get_buffer_size(Handle<Buffer> buffer_handle);
    template<typename T>
    inline T *map_buffer(Handle<Buffer> buffer_handle) { return reinterpret_cast<T*>(map_buffer(buffer_handle)); }
    void flush_buffer(Handle<Buffer> buffer_handle);

    // Global descriptor set
    void bind_global_uniform_buffer(Handle<Buffer> buffer_handle, usize offset, usize range);
    void bind_global_storage_image(u32 index, Handle<Image> image_handle);
    void bind_global_sampled_image(u32 index, Handle<Image> image_handle);
    inline Handle<Image> get_global_sampled_image(u32 index) { return get_image_descriptor(global_sets.sampled_images, index); }

    void update_globals();

    // Swapchain
    bool acquire_next_swapchain(Surface &surface);
    bool present(Surface &surface, Work &work);
};

}
