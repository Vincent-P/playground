#pragma once
#include <exo/algorithms.h>
#include <exo/handle.h>
#include <exo/collections/vector.h>
#include "render/vulkan/resources.h"
#include "vulkan/vulkan_core.h"

#include <vulkan/vulkan.h>
#include <array>


namespace vulkan
{
struct Device;
struct Image;
struct Surface;

struct QueryPool
{
    VkQueryPool vkhandle = VK_NULL_HANDLE;
};

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

// DX12-like fence used for CPU/CPU, CPU/GPU, or GPU/GPU synchronization
struct Fence
{
    VkSemaphore timeline_semaphore = VK_NULL_HANDLE;
    u64 value = 0;
};

// Command buffer / Queue abstraction
struct Work
{
    Device *device;

    VkCommandBuffer command_buffer;
    Vec<Fence> wait_fence_list;
    Vec<u64> wait_value_list;
    Vec<VkPipelineStageFlags> wait_stage_list;
    VkQueue queue;
    QueueType queue_type;

    // vulkan hacks:
    Option<VkSemaphore> image_acquired_semaphore;
    Option<VkPipelineStageFlags> image_acquired_stage;
    Option<VkSemaphore> signal_present_semaphore;

    void begin();
    void bind_global_set();
    void end();

    void wait_for(Fence &fence, u64 wait_value, VkPipelineStageFlags stage_dst);

    // vulkan hacks:
    void wait_for_acquired(Surface &surface, VkPipelineStageFlags stage_dst);
    void prepare_present(Surface &surface);

    void barrier(Handle<Buffer> buffer, BufferUsage usage_destination);
    void absolute_barrier(Handle<Image> image_handle);
    void barrier(Handle<Image> image, ImageUsage usage_destination);
    void clear_barrier(Handle<Image> image, ImageUsage usage_destination);
    void barriers(Vec<std::pair<Handle<Image>, ImageUsage>> images, Vec<std::pair<Handle<Buffer>, BufferUsage>> buffers);

    // queries
    void reset_query_pool(QueryPool &query_pool, u32 first_query, u32 count);
    void begin_query(QueryPool &query_pool, u32 index);
    void end_query(QueryPool &query_pool, u32 index);
    void timestamp_query(QueryPool &query_pool, u32 index);
};

struct TransferWork : Work
{
    void copy_buffer(Handle<Buffer> src, Handle<Buffer> dst, Vec<std::pair<u32, u32>> offsets_sizes);
    void copy_buffer(Handle<Buffer> src, Handle<Buffer> dst);
    void copy_buffer_to_image(Handle<Buffer> src, Handle<Image> dst);
    void fill_buffer(Handle<Buffer> buffer_handle, u32 data);
    void transfer();
};

struct ComputeWork : TransferWork
{
    void bind_pipeline(Handle<ComputeProgram> program_handle);
    void dispatch(uint3 workgroups);

    void clear_image(Handle<Image> image, VkClearColorValue clear_color);

    void bind_uniform_buffer(Handle<ComputeProgram> program_handle, u32 slot, Handle<Buffer> buffer_handle, u32 offset, usize size);
    void bind_uniform_buffer(Handle<GraphicsProgram> program_handle, u32 slot, Handle<Buffer> buffer_handle, u32 offset, usize size);
    void bind_storage_buffer(Handle<ComputeProgram> program_handle, u32 slot, Handle<Buffer> buffer_handle);
    void bind_storage_buffer(Handle<GraphicsProgram> program_handle, u32 slot, Handle<Buffer> buffer_handle);
    void bind_storage_image(Handle<ComputeProgram> program_handle, u32 slot, Handle<Image> image_handle);
    void bind_storage_image(Handle<GraphicsProgram> program_handle, u32 slot, Handle<Image> image_handle);

    void push_constant(const void *data, usize len);
    template<typename T>
    void push_constant(const T &object)
    {
        this->push_constant(&object, sizeof(T));
    }
};

struct GraphicsWork : ComputeWork
{
    struct DrawIndexedOptions
    {
        u32 vertex_count    = 0;
        u32 instance_count  = 1;
        u32 index_offset    = 0;
        i32 vertex_offset   = 0;
        u32 instance_offset = 0;
    };
    void draw_indexed(const DrawIndexedOptions &options);

    struct DrawOptions
    {
        u32 vertex_count    = 0;
        u32 instance_count  = 1;
        i32 vertex_offset   = 0;
        u32 instance_offset = 0;
    };
    void draw(const DrawOptions &options);

    void set_scissor(const VkRect2D &rect);
    void set_viewport(const VkViewport &viewport);

    void begin_pass(Handle<RenderPass> renderpass_handle, Handle<Framebuffer> framebuffer_handle, Vec<Handle<Image>> attachments, Vec<VkClearValue> clear_values);
    void end_pass();

    using ComputeWork::bind_pipeline; // make it visible on GraphicsWork
    void bind_pipeline(Handle<GraphicsProgram> program_handle, uint pipeline_index);
    void bind_index_buffer(Handle<Buffer> buffer_handle, VkIndexType index_type = VK_INDEX_TYPE_UINT16, u32 offset = 0);
};
}
