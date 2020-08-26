#pragma once

#include "renderer/vlk_context.hpp"
#include "types.hpp"

#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <array>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

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

inline constexpr u32 MAX_DESCRIPTOR_SET    = 3;
inline constexpr u32 GLOBAL_DESCRIPTOR_SET = 0;
inline constexpr u32 SHADER_DESCRIPTOR_SET = 1;
inline constexpr u32 DRAW_DESCRIPTOR_SET   = 2;

inline constexpr VkImageUsageFlags depth_attachment_usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
inline constexpr VkImageUsageFlags color_attachment_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
inline constexpr VkImageUsageFlags sampled_image_usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
inline constexpr VkImageUsageFlags storage_image_usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

struct ImageInfo
{
    const char *name                    = "No name";
    VkImageType type                    = VK_IMAGE_TYPE_2D;
    VkFormat format                     = VK_FORMAT_R8G8B8A8_UNORM;
    std::vector<VkFormat> extra_formats = {};
    u32 width                           = 1;
    u32 height                          = 1;
    u32 depth                           = 1;
    u32 mip_levels                      = 1;
    bool generate_mip_levels            = false;
    u32 layers                          = 1;
    VkSampleCountFlagBits samples       = VK_SAMPLE_COUNT_1_BIT;
    VkImageUsageFlags usages            = sampled_image_usage;
    VmaMemoryUsage memory_usage         = VMA_MEMORY_USAGE_GPU_ONLY;
    // sparse resident textures
    bool is_sparse = false;
    usize max_sparse_size;
    bool is_linear = false;

    bool operator==(const ImageInfo &) const = default;
};

struct Image;
using ImageH = Handle<Image>;
struct Image
{
    const char *name;

    ImageInfo info;

    // regular texture
    VkImage vkhandle;
    VmaAllocation allocation;

    // sparse residency texture
    usize page_size;
    std::vector<VmaAllocation> sparse_allocations;
    std::vector<VmaAllocationInfo> allocations_infos;

    VkImageLayout layout;
    VkImageSubresourceRange full_range;

    VkImageView default_view; // view with the default format (image_info.format) and full range
    std::vector<VkFormat> extra_formats;
    std::vector<VkImageView> format_views; // extra views for each image_info.extra_formats
    std::vector<VkImageView> mip_views;    // mip slices with defaut format

    VkSampler default_sampler;
    FatPtr mapped_ptr{};

    bool operator==(const Image &b) const = default;
};

struct SamplerInfo
{
    VkFilter mag_filter               = VK_FILTER_NEAREST;
    VkFilter min_filter               = VK_FILTER_NEAREST;
    VkSamplerMipmapMode mip_map_mode  = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
};

struct Sampler
{
    VkSampler vkhandle;
    SamplerInfo info;
};
using SamplerH = Handle<Sampler>;

struct BufferInfo
{
    const char *name            = "No name";
    usize size                  = 1;
    VkBufferUsageFlags usage    = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    VmaMemoryUsage memory_usage = VMA_MEMORY_USAGE_GPU_ONLY;
};

struct Buffer
{
    const char *name;
    VkBuffer vkhandle;
    VmaAllocation allocation;
    VmaMemoryUsage memory_usage;
    VkBufferUsageFlags usage;
    void *mapped;
    usize size;

    bool operator==(const Buffer &b) const = default;
};
using BufferH = Handle<Buffer>;

struct RTInfo
{
    bool is_swapchain = false;
    ImageH image_h    = {};
};

struct RenderTarget
{
    bool is_swapchain;
    ImageH image_h;
};

using RenderTargetH = Handle<RenderTarget>;

struct FrameBufferInfo
{
    VkImageView image_view;
    VkImageView depth_view;

    u32 width;
    u32 height;
    VkRenderPass render_pass; // renderpass info instead?

    bool operator==(const FrameBufferInfo &b) const = default;
};

struct FrameBuffer
{
    FrameBufferInfo info;
    VkFramebuffer vkhandle;
};

struct AttachmentInfo
{
    VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    RenderTargetH rt;

    bool operator==(const AttachmentInfo &b) const = default;
};

struct PassInfo
{
    // param for VkRenderPass
    bool present; // if it is the last pass and it should transition to present
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

    // param for VkFrameBuffer
    std::optional<AttachmentInfo> color;
    std::optional<AttachmentInfo> depth;

    bool operator==(const PassInfo &b) const = default;
};

struct RenderPass
{
    PassInfo info;
    VkRenderPass vkhandle;

    bool operator==(const RenderPass &b) const
    {
        return info == b.info && vkhandle == b.vkhandle;
    }
};

using RenderPassH = Handle<RenderPass>;

// Idea: Program contains different "configurations" coresponding to pipelines so that
// the HL API has a VkPipeline equivalent used to make sure they are created only during load time?
// maybe it is possible to deduce these configurations automatically from render graph, but render graph is
// created every frame

struct Shader
{
    std::string name;
    VkShaderModule vkhandle;

    bool operator==(const Shader &b) const = default;
};

using ShaderH = Handle<Shader>;

// replace with VkPushConstantRange?
// i like the name of this members as params
struct PushConstantInfo
{
    VkShaderStageFlags stages = VK_SHADER_STAGE_ALL;
    u32 offset                = 0;
    u32 size                  = 0;

    bool operator==(const PushConstantInfo &) const = default;
};

// replace with VkDescriptorSetLayoutBinding?
// i like the name of this members as params
struct BindingInfo
{
    u32 set                   = 0;
    u32 slot                  = 0;
    VkShaderStageFlags stages = VK_SHADER_STAGE_ALL;
    VkDescriptorType type     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    u32 count                 = 1;

    bool operator==(const BindingInfo &) const = default;
};

struct VertexInfo
{
    VkFormat format;
    u32 offset;

    bool operator==(const VertexInfo &) const = default;
};

struct VertexBufferInfo
{
    u32 stride;
    std::vector<VertexInfo> vertices_info;

    bool operator==(const VertexBufferInfo &) const = default;
};

struct GraphicsProgramInfo
{
    ShaderH vertex_shader;
    ShaderH geom_shader;
    ShaderH fragment_shader;

    std::vector<PushConstantInfo> push_constants;
    std::array<std::vector<BindingInfo>, MAX_DESCRIPTOR_SET> bindings_by_set;
    VertexBufferInfo vertex_buffer_info;

    // TODO: is it the right struct for this?
    // Either we create different programs in user code OR we implicitly handle them here
    // and keep track of dynamic states
    std::optional<VkCompareOp> depth_test;
    bool enable_depth_write{false};
    bool enable_conservative_rasterization{false};
    float depth_bias{0.0f};

    bool operator==(const GraphicsProgramInfo &) const = default;

    void push_constant(PushConstantInfo &&push_constant);
    void binding(BindingInfo &&binding);
    void vertex_stride(u32 value);
    void vertex_info(VertexInfo &&info);
};

struct ComputeProgramInfo
{
    ShaderH shader;

    std::vector<PushConstantInfo> push_constants;
    std::vector<BindingInfo> bindings;

    bool operator==(const ComputeProgramInfo &) const = default;

    void push_constant(PushConstantInfo &&push_constant);
    void binding(BindingInfo &&binding);
};

// TODO: smart fields
struct PipelineInfo
{
    GraphicsProgramInfo program_info;
    VkPipelineLayout pipeline_layout;
    RenderPassH render_pass;

    bool operator==(const PipelineInfo &) const = default;
    // bool operator==(const PipelineInfo &other) const { return program_info == other.program_info && render_pass ==
    // other.render_pass; }
};

struct DescriptorSet
{
    VkDescriptorSet set;
    usize frame_used;
};

struct ShaderBinding
{
    u32 binding;
    VkDescriptorType type;
    std::vector<VkDescriptorImageInfo> images_info;
    VkBufferView buffer_view;
    VkDescriptorBufferInfo buffer_info;

    bool operator==(const ShaderBinding &) const = default;
};

struct GraphicsProgram
{
    std::array<VkDescriptorSetLayout, MAX_DESCRIPTOR_SET> descriptor_layouts;
    VkPipelineLayout pipeline_layout; // layoutS??

    std::array<std::vector<std::optional<ShaderBinding>>, MAX_DESCRIPTOR_SET> binded_data_by_set;
    std::array<bool, MAX_DESCRIPTOR_SET> data_dirty_by_set;

    std::array<std::vector<DescriptorSet>, MAX_DESCRIPTOR_SET> descriptor_sets;
    std::array<usize, MAX_DESCRIPTOR_SET> current_descriptor_set;

    // TODO: hashmap
    std::vector<PipelineInfo> pipelines_info;
    std::vector<VkPipeline> pipelines_vk;

    GraphicsProgramInfo info;
    std::array<usize, MAX_DESCRIPTOR_SET> dynamic_count_by_set;

    bool operator==(const GraphicsProgram &b) const
    {
        return info == b.info;
    }
};

using GraphicsProgramH = Handle<GraphicsProgram>;

struct ComputeProgram
{
    VkDescriptorSetLayout descriptor_layout;
    VkPipelineLayout pipeline_layout; // layoutS??

    std::vector<std::optional<ShaderBinding>> binded_data;
    bool data_dirty;
    usize dynamic_count;

    std::vector<DescriptorSet> descriptor_sets;
    usize current_descriptor_set;

    ComputeProgramInfo info;
    std::vector<VkComputePipelineCreateInfo> pipelines_info;
    std::vector<VkPipeline> pipelines_vk;

    bool operator==(const ComputeProgram &b) const
    {
        return info == b.info;
    }
};

using ComputeProgramH = Handle<ComputeProgram>;

// temporary command buffer for the frame
struct CommandBuffer
{
    Context &ctx;
    VkCommandBuffer vkhandle;
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

struct Timestamp
{
    std::string_view label;
    float gpu_microseconds;
    float cpu_milliseconds;
};

struct API
{
    Context ctx;

    std::string_view current_label;
    std::vector<Timestamp> timestamps;
    std::vector<std::vector<TimePoint>> cpu_timestamps_per_frame;
    std::vector<std::vector<std::string_view>> timestamp_labels_per_frame;

    // resources
    Arena<Image> images;
    Arena<RenderTarget> rendertargets;
    Arena<Sampler> samplers;
    Arena<Buffer> buffers;
    Arena<GraphicsProgram> graphics_programs;
    Arena<ComputeProgram> compute_programs;
    Arena<Shader> shaders;

    // TODO: arena?
    std::vector<FrameBuffer> framebuffers;
    Arena<RenderPass> renderpasses;

    // Ring buffers for dynamic resources
    CircularBuffer staging_buffer;
    CircularBuffer dyn_uniform_buffer;
    CircularBuffer dyn_vertex_buffer;
    CircularBuffer dyn_index_buffer;

    // render context separate struct?
    RenderPassH current_render_pass;
    GraphicsProgram *current_program;

    static API create(const Window &window);
    void destroy();

    void on_resize(int width, int height);
    bool start_frame();
    void end_frame();
    bool start_present();
    void wait_idle();

    /// --- Drawing
    void begin_pass(PassInfo &&info);
    void end_pass();
    void bind_program(GraphicsProgramH H);

    void bind_image(GraphicsProgramH program_h, uint set, uint slot, ImageH image_h,
                    std::optional<VkImageView> image_view = std::nullopt);
    void bind_image(ComputeProgramH program_h, uint slot, ImageH image_h,
                    std::optional<VkImageView> image_view = std::nullopt);

    void bind_images(GraphicsProgramH program_h, uint set, uint slot, const std::vector<ImageH> &images_h,
                     const std::vector<VkImageView> &images_view);
    void bind_images(ComputeProgramH program_h, uint slot, const std::vector<ImageH> &images_h,
                     const std::vector<VkImageView> &images_view);

    void bind_combined_image_sampler(GraphicsProgramH program_h, uint set, uint slot, ImageH image_h,
                                     SamplerH sampler_h, std::optional<VkImageView> image_view = std::nullopt);
    void bind_combined_image_sampler(ComputeProgramH program_h, uint slot, ImageH image_h, SamplerH sampler_h,
                                     std::optional<VkImageView> image_view = std::nullopt);

    void bind_combined_images_sampler(GraphicsProgramH program_h, uint set, uint slot,
                                      const std::vector<ImageH> &images_h, SamplerH sampler_h,
                                      const std::vector<VkImageView> &images_view);
    void bind_combined_images_sampler(ComputeProgramH program_h, uint slot, const std::vector<ImageH> &images_h,
                                      SamplerH sampler_h, const std::vector<VkImageView> &images_view);

    void bind_buffer(GraphicsProgramH program_h, uint set, uint slot, CircularBufferPosition buffer_pos);
    void bind_buffer(ComputeProgramH program_h, uint slot, CircularBufferPosition buffer_pos);
    void dispatch(ComputeProgramH program_h, u32 x, u32 y, u32 z);

    /// --- Debug
    void begin_label(std::string_view name, float4 color = {1, 1, 1, 1});
    void end_label();
    void global_barrier();

    template <typename T> T *bind_uniform()
    {
        auto pos = map_circular_buffer_internal(*this, staging_buffer, sizeof(T));
        return pos.mapped;
    }

    void bind_vertex_buffer(BufferH H, u32 offset = 0);
    void bind_vertex_buffer(CircularBufferPosition v_pos);
    void bind_index_buffer(BufferH H, u32 offset = 0);
    void bind_index_buffer(CircularBufferPosition i_pos);
    void push_constant(VkShaderStageFlagBits stage, u32 offset, u32 size, void *data);

    void draw(u32 vertex_count, u32 instance_count, u32 first_vertex, u32 first_instance);
    void draw_indexed(u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset, u32 first_instance);

    void set_scissor(const VkRect2D &scissor);
    void set_viewport(const VkViewport &viewport);
    void set_viewport_and_scissor(u32 width, u32 height);

    void clear_image(ImageH H, const VkClearColorValue &clear_color);

    /// ---
    CircularBufferPosition copy_to_staging_buffer(void *data, usize len);
    CircularBufferPosition dynamic_vertex_buffer(usize len);
    CircularBufferPosition dynamic_index_buffer(usize len);
    CircularBufferPosition dynamic_uniform_buffer(usize len);

    /// --- Resources
    // Images
    ImageH create_image(const ImageInfo &info);
    Image &get_image(ImageH H);
    void destroy_image(ImageH H);

    void upload_image(ImageH H, void *data, usize len);
    void generate_mipmaps(ImageH H);
    FatPtr read_image(ImageH H);

    // Samplers
    SamplerH create_sampler(const SamplerInfo &info);
    Sampler &get_sampler(SamplerH H);
    void destroy_sampler(SamplerH H);

    // Render targets
    RenderTargetH create_rendertarget(const RTInfo &info);
    RenderTarget &get_rendertarget(RenderTargetH H);
    void destroy_rendertarget(RenderTargetH H);

    // Buffers
    BufferH create_buffer(const BufferInfo &info);
    Buffer &get_buffer(BufferH H);
    void destroy_buffer(BufferH H);
    void upload_buffer(BufferH H, void *data, usize len);

    // Shaders
    ShaderH create_shader(std::string_view path);
    Shader &get_shader(ShaderH H);
    void destroy_shader(ShaderH H);

    // Programs
    GraphicsProgramH create_program(GraphicsProgramInfo &&info);
    ComputeProgramH create_program(ComputeProgramInfo &&info);
    GraphicsProgram &get_program(GraphicsProgramH H);
    ComputeProgram &get_program(ComputeProgramH H);
    void destroy_program(GraphicsProgramH H);
    void destroy_program(ComputeProgramH H);

    // COmmand buffers
    CommandBuffer get_temp_cmd_buffer();

    /// --- Queries
    void add_timestamp(std::string_view label);
};

void destroy_buffer_internal(API &api, Buffer &buffer);
void destroy_image_internal(API &api, Image &img);
void destroy_sampler_internal(API &api, Sampler &img);
void destroy_program_internal(API &api, GraphicsProgram &program);
void destroy_program_internal(API &api, ComputeProgram &program);
void destroy_shader_internal(API &api, Shader &program);

} // namespace vulkan
} // namespace my_app
