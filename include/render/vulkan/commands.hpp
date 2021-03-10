#pragma once
#include "base/handle.hpp"
#include "base/vector.hpp"
#include "render/vulkan/resources.hpp"
#include "vulkan/vulkan_core.h"

#include <vulkan/vulkan.h>


namespace vulkan
{
struct Device;
struct Image;
struct Surface;

// A request to send a resource to another queue
// TODO:
// ResourceTransfer t = cmd1.send_to(image1, cmd2);
// cmd1.receive(t);
struct ResourceTransfer
{
    int sender;
    int receiver;
    int resource;
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
    void end();

    // TODO:
    ResourceTransfer send_to(int receiver, int resource);
    void receive(ResourceTransfer transfer);

    void wait_for(Fence &fence, u64 wait_value, VkPipelineStageFlags stage_dst);

    // vulkan hacks:
    void wait_for_acquired(Surface &surface, VkPipelineStageFlags stage_dst);
    void prepare_present(Surface &surface);

    void barrier(Handle<Image> image, ImageUsage usage_destination);
    void clear_barrier(Handle<Image> image, ImageUsage usage_destination);
    void barriers(Vec<std::pair<Handle<Image>, ImageUsage>> images, Vec<std::pair<Handle<Buffer>, BufferUsage>> buffers);
};

struct TransferWork : Work
{
    void copy_buffer(Handle<Buffer> src, Handle<Buffer> dst);
    void copy_buffer_to_image(Handle<Buffer> src, Handle<Image> dst);
    void fill_buffer(Handle<Buffer> buffer_handle, u32 data);
    void transfer();
};

struct ComputeWork : TransferWork
{
    void clear_image(Handle<Image> image, VkClearColorValue clear_color);

    void dispatch();
    void bind_pipeline(Handle<ComputeProgram> program_handle);


    void bind_uniform_buffer(Handle<GraphicsProgram> program_handle, u32 slot, Handle<Buffer> buffer_handle, usize offset, usize size);
    void bind_image(Handle<GraphicsProgram> program_handle, uint slot, Handle<Image> image_handle);
};

struct GraphicsWork : ComputeWork
{
    struct DrawIndexedOptions
    {
        u32 vertex_count = 0;
        u32 instance_count = 1;
        u32 index_offset = 0;
        i32 vertex_offset = 0;
        u32 instance_offset = 0;
    };
    void draw_indexed(const DrawIndexedOptions &options);

    void set_scissor(const VkRect2D &rect);
    void set_viewport(const VkViewport &viewport);

    void begin_pass(Handle<RenderPass> renderpass_handle, Handle<Framebuffer> framebuffer_handle, Vec<Handle<Image>> attachments, Vec<VkClearValue> clear_values);
    void end_pass();
    void bind_pipeline(Handle<GraphicsProgram> program_handle, uint pipeline_index);
    void bind_index_buffer(Handle<Buffer> buffer_handle);
};
}
