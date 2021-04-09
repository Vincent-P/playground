#pragma once
#include "base/algorithms.hpp"
#include "base/types.hpp"
#include "base/option.hpp"
#include "base/vector.hpp"
#include "base/pool.hpp"

#include "render/vulkan/context.hpp"
#include "render/vulkan/commands.hpp"
#include "render/vulkan/queues.hpp"
#include "render/vulkan/resources.hpp"
#include "render/vulkan/descriptor_set.hpp"

#include <array>
#include <utility>
#include <string_view>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace vulkan
{
struct Surface;

struct CommandPool
{
    VkCommandPool vk_handle = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> free_list = {};
};

struct WorkPool
{
    std::array<CommandPool, 3> command_pools;

    inline CommandPool &graphics() { return command_pools[to_underlying(QueueType::Graphics)]; }
    inline CommandPool &compute()  { return command_pools[to_underlying(QueueType::Compute)];  }
    inline CommandPool &transfer() { return command_pools[to_underlying(QueueType::Transfer)]; }
};

enum BuiltinSampler
{
    Default = 0,
    Count
};

struct GlobalDescriptorSet
{
    VkDescriptorSet vkset;
    VkDescriptorSetLayout vklayout;
    VkDescriptorPool vkpool;
    VkPipelineLayout vkpipelinelayout;

    Handle<Buffer> dynamic_buffer;
    u32 dynamic_offset = 0;
    u32 dynamic_range = 0;

    Vec<Handle<Image>> storage_images;
    Vec<Handle<Image>> sampled_images;
    u32 current_sampled_image = 0;
    u32 current_storage_image = 0;

    Vec<Handle<Image>> pending_images;
    Vec<u32> pending_indices;
    Vec<u32> pending_binding;
    Handle<Buffer> pending_buffer;
    u32 pending_offset;
    u32 pending_range;
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
    GlobalDescriptorSet global_set;

    Pool<Shader> shaders;
    Pool<GraphicsProgram> graphics_programs;
    Pool<ComputeProgram> compute_programs;
    Pool<RenderPass> renderpasses;
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

    Handle<RenderPass> create_renderpass(const RenderAttachments &render_attachments);
    Handle<RenderPass> find_or_create_renderpass(const RenderAttachments &render_attachments);
    Handle<RenderPass> find_or_create_renderpass(Handle<Framebuffer> framebuffer_handle);
    void destroy_renderpass(Handle<RenderPass> renderpass_handle);

    Handle<Framebuffer> create_framebuffer(const FramebufferDescription &desc);
    Handle<Framebuffer> find_or_create_framebuffer(const FramebufferDescription &desc);
    void destroy_framebuffer(Handle<Framebuffer> framebuffer_handle);

    // Compute pipeline
    void recreate_program_internal(ComputeProgram &compute_program);
    Handle<ComputeProgram> create_program(std::string name, const ComputeState &compute_state);
    void destroy_program(Handle<ComputeProgram> program_handle);

    // Resources
    Handle<Image> create_image(const ImageDescription &image_desc, Option<VkImage> proxy = {});
    void destroy_image(Handle<Image> image_handle);
    u32 get_image_sampled_index(Handle<Image> image_handle);
    u32 get_image_storage_index(Handle<Image> image_handle);
    uint3 get_image_size(Handle<Image> image_handle);

    Handle<Buffer> create_buffer(const BufferDescription &buffer_desc);
    void destroy_buffer(Handle<Buffer> buffer_handle);

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
    inline Handle<Image> get_global_sampled_image(u32 index) { return global_set.sampled_images[index]; }
    void update_globals();

    // Swapchain
    bool acquire_next_swapchain(Surface &surface);
    bool present(Surface &surface, Work &work);
};

}
