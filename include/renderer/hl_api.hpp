#pragma once

#include "renderer/vlk_context.hpp"
#include "types.hpp"
#include <optional>
#include <thsvs/thsvs_simpler_vulkan_synchronization.h>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

/***
 * The HL API is a Vulkan abstraction.
 * It contains high-level struct and classes over vulkan
 * - Shaders/Programs: Abstract all the descriptor layouts, bindings, and pipelines manipulation
 * - Render Target: Abstract everything related to the render passes and framebuffer
 * - Textures/Buffers: Abstract resources
 ***/

namespace my_app
{
namespace vulkan
{

struct ImageInfo
{
    const char *name;
    vk::ImageType type = vk::ImageType::e2D;
    vk::Format format  = vk::Format::eR8G8B8A8Unorm;
    u32 width;
    u32 height;
    u32 depth;
    u32 mip_levels                  = 1;
    u32 layers                      = 1;
    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
    vk::ImageUsageFlags usages = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
};

struct Image
{
    const char *name;
    vk::Image vkhandle;
    vk::ImageCreateInfo image_info;
    VmaAllocation allocation;
    VmaMemoryUsage memory_usage;
    ThsvsAccessType access;
    vk::ImageSubresourceRange full_range;
    vk::ImageView default_view;
    vk::Sampler default_sampler;
};
using ImageH = Handle<Image>;

struct Sampler
{
    vk::UniqueSampler vkhandle;
};

struct BufferInfo
{
    const char *name;
    usize size;
    vk::BufferUsageFlags usage;
    VmaMemoryUsage memory_usage;
};

struct Buffer
{
    const char *name;
    vk::Buffer vkhandle;
    VmaAllocation allocation;
    VmaMemoryUsage memory_usage;
    vk::BufferUsageFlags usage;
    void *mapped;
    usize size;
};
using BufferH = Handle<Buffer>;

struct RTInfo
{
    bool is_swapchain;
    ImageH image_h;
};

struct RenderTarget
{
    bool is_swapchain;
    ImageH image_h;
};

using RenderTargetH = Handle<RenderTarget>;

struct FrameBufferInfo
{
    vk::ImageView image_view;
    vk::ImageView depth_view;

    vk::RenderPass render_pass; // renderpass info instead?
};
inline bool operator==(const FrameBufferInfo &a, const FrameBufferInfo &b)
{
    return a.image_view == b.image_view && a.render_pass == b.render_pass;
}

struct FrameBuffer
{
    FrameBufferInfo info;
    vk::UniqueFramebuffer vkhandle;
};

struct AttachmentInfo
{
    vk::AttachmentLoadOp load_op = vk::AttachmentLoadOp::eDontCare;
    RenderTargetH rt;
};

inline bool operator==(const AttachmentInfo &a, const AttachmentInfo &b)
{
    return a.load_op == b.load_op && a.rt == b.rt;
}

struct PassInfo
{
    bool present; // if it is the last pass and it should transition to present
    AttachmentInfo color;
    std::optional<AttachmentInfo> depth;
};

inline bool operator==(const PassInfo &a, const PassInfo &b)
{
    return a.present == b.present && a.color == b.color && a.depth == b.depth;
}

struct RenderPass
{
    PassInfo info;
    vk::UniqueRenderPass vkhandle;
};
using RenderPassH = Handle<RenderPass>;

// Idea: Program contains different "configurations" coresponding to pipelines so that
// the HL API has a VkPipeline equivalent used to make sure they are created only during load time?
// maybe it is possible to deduce these configurations automatically from render graph, but render graph is
// created every frame

struct Shader
{
    const char *name;
    vk::UniqueShaderModule vkhandle;
};

using ShaderH = Handle<Shader>;

// replace with vk::PushConstantRange?
// i like the name of this members as params
struct PushConstantInfo
{
    vk::ShaderStageFlags stages;
    u32 offset;
    u32 size;
};

inline bool operator==(const PushConstantInfo &a, const PushConstantInfo &b)
{
    return a.stages == b.stages && a.offset == b.offset && a.size == b.size;
}

// replace with vk::DescriptorSetLayoutBinding?
// i like the name of this members as params
struct BindingInfo
{
    u32 slot;
    vk::ShaderStageFlags stages;
    vk::DescriptorType type;
    u32 count;
};

inline bool operator==(const BindingInfo &a, const BindingInfo &b)
{
    return a.slot == b.slot && a.stages == b.stages && a.type == b.type && a.count == b.count;
}

struct VertexInfo
{
    vk::Format format;
    u32 offset;
};

inline bool operator==(const VertexInfo &a, const VertexInfo &b)
{
    return a.format == b.format && a.offset == b.offset;
}

struct VertexBufferInfo
{
    u32 stride;
    std::vector<VertexInfo> vertices_info;
};

inline bool operator==(const VertexBufferInfo &a, const VertexBufferInfo &b)
{
    return a.stride == b.stride && a.vertices_info == b.vertices_info;
}

struct ProgramInfo
{
    ShaderH vertex_shader;
    ShaderH fragment_shader;
    std::vector<PushConstantInfo> push_constants;
    std::vector<BindingInfo> bindings;
    VertexBufferInfo vertex_buffer_info;
    bool enable_depth;

    void push_constant(PushConstantInfo &&push_constant);
    void binding(BindingInfo &&binding);
    void vertex_stride(u32 value);
    void vertex_info(VertexInfo &&info);
};

inline bool operator==(const ProgramInfo &a, const ProgramInfo &b)
{
    return a.vertex_shader == b.vertex_shader && a.fragment_shader == b.fragment_shader
           && a.push_constants == b.push_constants && a.bindings == b.bindings
           && a.vertex_buffer_info == b.vertex_buffer_info;
}

// TODO: smart fields
struct PipelineInfo
{
    ProgramInfo program_info;
    vk::PipelineLayout pipeline_layout;
    vk::RenderPass vk_render_pass;

    bool operator==(const PipelineInfo &other) { return program_info == other.program_info; }
};

struct DescriptorSet
{
    vk::DescriptorSet set;
    usize frame_used;
};

struct ShaderBinding
{
    u32 binding;
    vk::DescriptorType type;
    vk::DescriptorImageInfo image_info;
    vk::BufferView buffer_view;
    vk::DescriptorBufferInfo buffer_info;
};

inline bool operator==(const ShaderBinding &a, const ShaderBinding &b)
{
    return a.binding == b.binding && a.type == b.type && a.image_info == b.image_info && a.buffer_view == b.buffer_view
           && a.buffer_info == b.buffer_info;
}

inline bool operator!=(const ShaderBinding &a, const ShaderBinding &b) { return !(a == b); }

struct Program
{
    vk::UniqueDescriptorSetLayout descriptor_layout;
    vk::UniquePipelineLayout pipeline_layout; // layoutS??

    std::vector<std::optional<ShaderBinding>> binded_data;
    bool data_dirty{true};

    std::vector<DescriptorSet> descriptor_sets;
    usize current_descriptor_set;

    // TODO: hashmap
    std::vector<PipelineInfo> pipelines_info;
    std::vector<vk::UniquePipeline> pipelines_vk;

    ProgramInfo info;
    usize dynamic_count;
};

using ProgramH = Handle<Program>;

// temporary command buffer for the frame
struct CommandBuffer
{
    Context &ctx;
    vk::UniqueCommandBuffer vkhandle;
    void begin();
    void submit_and_wait();
};

struct CircularBufferPosition
{
    BufferH buffer_h;
    usize offset;
    usize length;
    void *mapped;
};

struct CircularBuffer
{
    BufferH buffer_h;
    usize offset;
};

struct API;

CircularBufferPosition map_circular_buffer_internal(API &api, CircularBuffer &circular, usize len);

struct API
{
    Context ctx;

    // todo: pool/arena data structure
    std::vector<Image> images;
    std::vector<RenderTarget> rendertargets;
    std::vector<Sampler> samplers;
    std::vector<Buffer> buffers;
    std::vector<FrameBuffer> framebuffers;
    std::vector<RenderPass> renderpasses;
    std::vector<Program> programs;
    std::vector<Shader> shaders;

    CircularBuffer staging_buffer;
    CircularBuffer dyn_uniform_buffer;
    CircularBuffer dyn_vertex_buffer;
    CircularBuffer dyn_index_buffer;

    // render context
    RenderPass *current_render_pass;
    Program *current_program;

    static API create(const Window &window);
    void destroy();

    void draw(); // TODO: used to make the HL API before the RenderGraph, remove once it's done

    void on_resize(int width, int height);
    bool start_frame();
    void end_frame();
    void wait_idle();

    /// --- Drawing
    void begin_pass(PassInfo &&info);
    void end_pass();
    void bind_program(ProgramH H);
    void bind_image(ProgramH program_h, uint slot, ImageH image_h);
    void bind_buffer(ProgramH program_h, uint slot, CircularBufferPosition buffer_pos);

    template<typename T>
    T* bind_uniform()
    {
        auto pos = map_circular_buffer_internal(*this, staging_buffer, sizeof(T));
        return pos.mapped;
    }

    void bind_vertex_buffer(BufferH H, u32 offset = 0);
    void bind_vertex_buffer(CircularBufferPosition v_pos);
    void bind_index_buffer(BufferH H, u32 offset = 0);
    void bind_index_buffer(CircularBufferPosition i_pos);
    void push_constant(vk::ShaderStageFlagBits stage, u32 offset, u32 size, void *data);

    void draw_indexed(u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset, u32 first_instance);
    void set_scissor(const vk::Rect2D &scissor);
    void set_viewport(const vk::Viewport &viewport);

    /// ---
    CircularBufferPosition copy_to_staging_buffer(void *data, usize len);
    CircularBufferPosition dynamic_vertex_buffer(usize len);
    CircularBufferPosition dynamic_index_buffer(usize len);
    CircularBufferPosition dynamic_uniform_buffer(usize len);

    /// --- Resources
    ImageH create_image(const ImageInfo &info);
    Image &get_image(ImageH H);
    void destroy_image(ImageH H);
    void upload_image(ImageH H, void *data, usize len);

    RenderTargetH create_rendertarget(const RTInfo &info);
    RenderTarget &get_rendertarget(RenderTargetH H);

    BufferH create_buffer(const BufferInfo &info);
    Buffer &get_buffer(BufferH H);
    void destroy_buffer(BufferH H);
    void upload_buffer(BufferH H, void *data, usize len);

    ShaderH create_shader(const char *path);
    Shader &get_shader(ShaderH H);
    void destroy_shader(ShaderH H);

    ProgramH create_program(ProgramInfo &&info);
    Program &get_program(ProgramH H);
    void destroy_program(ProgramH H);

    CommandBuffer get_temp_cmd_buffer();
};

} // namespace vulkan
} // namespace my_app
