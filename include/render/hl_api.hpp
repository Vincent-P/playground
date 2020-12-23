#pragma once

#include "render/vlk_context.hpp"
#include "base/types.hpp"
#include "base/pool.hpp"
#include "base/time.hpp"

#include <optional>
#include <string>
#include <string_view>
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
namespace UI { struct Context; }
namespace vulkan
{

inline constexpr u32 GLOBAL_DESCRIPTOR_SET = 0;
inline constexpr u32 SHADER_DESCRIPTOR_SET = 1;
inline constexpr u32 DRAW_DESCRIPTOR_SET   = 2;
inline constexpr u32 MAX_DESCRIPTOR_SET    = 2; // per shader!

inline constexpr VkImageUsageFlags depth_attachment_usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
inline constexpr VkImageUsageFlags color_attachment_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
inline constexpr VkImageUsageFlags sampled_image_usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
inline constexpr VkImageUsageFlags storage_image_usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

inline constexpr VkBufferUsageFlags storage_buffer_usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

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

    bool operator==(const ImageInfo &) const = default;
};

struct ImageAccess
{
    VkPipelineStageFlags stage = 0;
    VkAccessFlags access       = 0;
    VkImageLayout layout       = VK_IMAGE_LAYOUT_UNDEFINED;
    // queue?
};

enum struct ImageUsage
{
    None,
    GraphicsShaderRead,
    GraphicsShaderReadWrite,
    ComputeShaderRead,
    ComputeShaderReadWrite,
    TransferDst,
    TransferSrc,
    ColorAttachment,
    DepthAttachment,
    Present
};

struct Image;
using ImageH = Handle<Image>;
struct ImageView;
using ImageViewH = Handle<ImageView>;

struct Image
{
    const char *name = "";

    ImageInfo info;

    VkImage vkhandle = VK_NULL_HANDLE;
    VmaAllocation allocation;

    ImageUsage usage = ImageUsage::None;
    VkImageSubresourceRange full_range;

    std::vector<VkFormat> extra_formats;

    ImageViewH default_view; // view with the default format (image_info.format) and full range
    std::vector<ImageViewH> format_views; // extra views for each image_info.extra_formats
    std::vector<ImageViewH> mip_views;    // mip slices with defaut format

    // A proxy is an Image containing a VkImage that is external to the API (swapchain images for example)
    bool is_proxy = false;

    bool operator==(const Image &b) const = default;
};

struct ImageView
{
    ImageView() = default;

    ImageH image_h;
    VkImageSubresourceRange range;
    VkFormat format;
    VkImageViewType view_type;
    VkImageView vkhandle;

    bool operator==(const ImageView &b) const = default;
};

struct SamplerInfo
{
    VkFilter mag_filter               = VK_FILTER_NEAREST;
    VkFilter min_filter               = VK_FILTER_NEAREST;
    VkSamplerMipmapMode mip_map_mode  = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;

    bool operator==(const SamplerInfo &b) const = default;
};

struct Sampler
{
    Sampler() = default;

    VkSampler vkhandle;
    SamplerInfo info;

    bool operator==(const Sampler &b) const
    {
        return vkhandle == b.vkhandle && info == b.info;
    }
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

struct FrameBuffer
{
    VkFramebufferCreateInfo create_info;
    VkFramebuffer vkhandle;
};

struct AttachmentInfo
{
    VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ImageViewH image_view;

    bool operator==(const AttachmentInfo &b) const
    {
        return load_op == b.load_op && image_view == b.image_view;
    }
};

struct PassInfo
{
    // param for VkRenderPass
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

    // param for VkFrameBuffer
    std::vector<AttachmentInfo> colors = {};
    std::optional<AttachmentInfo> depth = {};

    bool operator==(const PassInfo &b) const
    {
        return samples == b.samples && colors == b.colors && depth == b.depth;
    }

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

using RenderPassH = usize;

// Idea: Program contains different "configurations" coresponding to pipelines so that
// the HL API has a VkPipeline equivalent used to make sure they are created only during load time?
// maybe it is possible to deduce these configurations automatically from render graph, but render graph is
// created every frame

struct Shader
{
    std::string name;
    VkShaderModule vkhandle;
    std::vector<u8> bytecode;

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

enum struct PrimitiveTopology
{
    TriangleList,
    PointList
};

struct DepthState
{
    std::optional<VkCompareOp> test = std::nullopt;
    bool enable_write = false;
    float bias = 0.0f;

    bool operator==(const DepthState &) const = default;
};

struct RasterizationState
{
    bool enable_conservative_rasterization{false};
    bool culling{true};

    bool operator==(const RasterizationState &) const = default;
};

struct InputAssemblyState
{
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    bool operator==(const InputAssemblyState &) const = default;
};

struct GraphicsProgramInfo
{
    ShaderH vertex_shader;
    ShaderH geom_shader;
    ShaderH fragment_shader;

    std::vector<PushConstantInfo> push_constants;
    VertexBufferInfo vertex_buffer_info;

    DepthState depth;
    RasterizationState rasterization;
    InputAssemblyState input_assembly;
    bool alpha_blending = false;

    bool operator==(const GraphicsProgramInfo &) const = default;

    void push_constant(PushConstantInfo &&push_constant);
    void vertex_stride(u32 value);
    void vertex_info(VertexInfo &&info);
};

struct ComputeProgramInfo
{
    ShaderH shader;

    std::vector<PushConstantInfo> push_constants;

    bool operator==(const ComputeProgramInfo &) const = default;

    void push_constant(PushConstantInfo &&push_constant);
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

struct BindingData
{
    std::vector<VkDescriptorImageInfo> images_info;
    VkBufferView buffer_view;
    VkDescriptorBufferInfo buffer_info;

    bool operator==(const BindingData &) const = default;
};

// A list of shader binding, basically abstracts a descriptor set
struct ShaderBindingSet
{
    VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;
    std::vector<DescriptorSet> descriptor_sets = {};
    usize current_descriptor_set = 0;

    std::vector<BindingInfo> bindings_info = {};
    std::vector<std::optional<BindingData>> binded_data = {};
    bool data_dirty = true; // dirty flag for each data to avoid updating everything?
    std::vector<u32> dynamic_offsets = {};
    std::vector<u32> dynamic_bindings = {};

    inline DescriptorSet &get_descriptor_set() { return descriptor_sets[current_descriptor_set]; }
};

inline DescriptorSet &get_descriptor_set(ShaderBindingSet& binding_set) { return binding_set.descriptor_sets[binding_set.current_descriptor_set]; }
void init_binding_set(Context &ctx, ShaderBindingSet &binding_set);

struct GraphicsProgram
{
    std::array<ShaderBindingSet, MAX_DESCRIPTOR_SET> binding_sets_by_freq;

    VkPipelineLayout pipeline_layout; // layoutS??
    // TODO: hashmap
    std::vector<PipelineInfo> pipelines_info;
    std::vector<VkPipeline> pipelines_vk;

    GraphicsProgramInfo info;

    bool operator==(const GraphicsProgram &b) const
    {
        return info == b.info;
    }
};

using GraphicsProgramH = Handle<GraphicsProgram>;

struct ComputeProgram
{
    ShaderBindingSet binding_set;

    ComputeProgramInfo info;
    VkPipelineLayout pipeline_layout; // layoutS??
    VkComputePipelineCreateInfo pipeline_info;
    VkPipeline pipeline_vk;
    u64 pipeline_shader;

    bool operator==(const ComputeProgram &b) const
    {
        return info == b.info;
    }
};

struct GlobalBindings
{
    ShaderBindingSet binding_set;
    void binding(BindingInfo &&binding);
};

using ComputeProgramH = Handle<ComputeProgram>;

// temporary command buffer for the frame
struct CommandBuffer
{
    Context &ctx;
    VkCommandBuffer vkhandle;
    void begin() const;
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
    Pool<Image> images;
    std::vector<ImageH> swapchain_to_image_h;

    Pool<ImageView> image_views;

    Pool<Sampler> samplers;
    SamplerH default_sampler;

    Pool<Buffer> buffers;

    Pool<Shader> shaders;

    GlobalBindings global_bindings;
    Pool<GraphicsProgram> graphics_programs;
    Pool<ComputeProgram> compute_programs;

    // TODO: arena?
    std::vector<FrameBuffer> framebuffers;
    std::vector<RenderPass> renderpasses;

    // Ring buffers for dynamic resources
    CircularBuffer staging_buffer;
    CircularBuffer dyn_uniform_buffer;
    CircularBuffer dyn_vertex_buffer;
    CircularBuffer dyn_index_buffer;

    // render context separate struct?
    RenderPassH current_render_pass;
    GraphicsProgram *current_program;

    // stats
    usize barriers_this_frame = 0;
    usize draws_this_frame = 0;
    usize graphics_pipeline_count = 0;
    usize compute_pipeline_count = 0;

    static void create(API& api, const platform::Window &window);
    void destroy();

    void on_resize(int window_width, int window_height);
    bool start_frame();
    void end_frame();
    bool start_present();
    void wait_idle() const;
    void display_ui(UI::Context &ui) const;

    /// --- Drawing
    void begin_pass(PassInfo &&info);
    void end_pass();
    void bind_program(GraphicsProgramH H);

    // storage images
    void bind_image(GraphicsProgramH program_h, ImageViewH image_view_h, uint set, uint slot, uint index = 0);
    void bind_image(ComputeProgramH program_h, ImageViewH image_view_h, uint slot, uint index = 0);
    void bind_images(GraphicsProgramH program_h, const std::vector<ImageViewH> &image_views_h, uint set, uint slot);
    void bind_images(ComputeProgramH program_h, const std::vector<ImageViewH> &image_views_h, uint slot);

    void bind_image(GraphicsProgramH program_h, ImageH image_h, uint set, uint slot, uint index = 0);
    void bind_image(ComputeProgramH program_h, ImageH image_h, uint slot, uint index = 0);
    void bind_images(GraphicsProgramH program_h, const std::vector<ImageH> &images_h, uint set, uint slot);
    void bind_images(ComputeProgramH program_h, const std::vector<ImageH> &images_h, uint slot);

    // sampled images
    void bind_combined_image_sampler(GraphicsProgramH program_h, ImageViewH image_view_h, SamplerH sampler_h, uint set, uint slot, uint index = 0);
    void bind_combined_image_sampler(ComputeProgramH program_h, ImageViewH image_view_h, SamplerH sampler_h, uint slot, uint index = 0);

    void bind_combined_images_samplers(GraphicsProgramH program_h, const std::vector<ImageViewH> &image_views_h,
                                       const std::vector<SamplerH> &samplers, uint set, uint slot);
    void bind_combined_images_samplers(ComputeProgramH program_h, const std::vector<ImageViewH> &image_views_h,
                                       const std::vector<SamplerH> &samplers, uint slot);

    void bind_combined_image_sampler(GraphicsProgramH program_h, ImageH image_h, SamplerH sampler_h, uint set, uint slot, uint index = 0);
    void bind_combined_image_sampler(ComputeProgramH program_h, ImageH image_h, SamplerH sampler_h, uint slot, uint index = 0);

    void bind_combined_images_samplers(GraphicsProgramH program_h, const std::vector<ImageH> &images_h,
                                       const std::vector<SamplerH> &samplers, uint set, uint slot);
    void bind_combined_images_samplers(ComputeProgramH program_h, const std::vector<ImageH> &images_h,
                                       const std::vector<SamplerH> &samplers, uint slot);

    // dynamic buffers
    void bind_buffer(GraphicsProgramH program_h, CircularBufferPosition buffer_pos, uint set, uint slot);
    void bind_buffer(ComputeProgramH program_h, CircularBufferPosition buffer_pos, uint slot);
    void bind_buffer(GraphicsProgramH program_h, BufferH buffer_h, uint set, uint slot);
    void bind_buffer(ComputeProgramH program_h, BufferH buffer_h, uint slot);

    void create_global_set();
    void update_global_set();

    void dispatch(ComputeProgramH program_h, uint3 size);
    uint3 dispatch_size(ImageH target, uint threads_xy = 1, uint threads_z = 1);

    /// --- Debug
    void begin_label(std::string_view name, float4 color = {1, 1, 1, 1});
    void end_label();

    template <typename T> T *bind_uniform()
    {
        auto pos = map_circular_buffer_internal(*this, staging_buffer, sizeof(T));
        return pos.mapped;
    }

    void bind_vertex_buffer(BufferH H, u32 offset = 0);
    void bind_vertex_buffer(CircularBufferPosition v_pos);
    void bind_index_buffer(BufferH H, u32 offset = 0);
    void bind_index_buffer(CircularBufferPosition i_pos);
    void push_constant(VkShaderStageFlags stage, u32 offset, u32 size, void *data);

    void draw(u32 vertex_count, u32 instance_count, u32 first_vertex, u32 first_instance);
    void draw_indexed(u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset, u32 first_instance);

    void set_scissor(const VkRect2D &scissor);
    void set_viewport(const VkViewport &viewport);
    void set_viewport_and_scissor(u32 width, u32 height);

    void clear_image(ImageH H, const VkClearColorValue &clear_color);
    void clear_buffer(BufferH H, u32 data);
    void clear_buffer(BufferH H, float data);

    /// ---
    CircularBufferPosition copy_to_staging_buffer(void *data, usize len);
    CircularBufferPosition dynamic_vertex_buffer(usize len);
    CircularBufferPosition dynamic_index_buffer(usize len);
    CircularBufferPosition dynamic_uniform_buffer(usize len);

    /// --- Resources
    // Images
    ImageH create_image(const ImageInfo &info);
    ImageH create_image_proxy(VkImage external, const ImageInfo &info);

    Image &get_image(ImageH H);

    inline ImageH get_current_swapchain_h() const { return swapchain_to_image_h[ctx.swapchain.current_image]; };
    inline Image &get_current_swapchain() { return get_image(get_current_swapchain_h()); };

    void destroy_image(ImageH H);

    void upload_image(ImageH H, void *data, usize len);
    void generate_mipmaps(ImageH H);
    void transfer_done(ImageH H); // it's a hack for now

    ImageView &get_image_view(ImageViewH H);


    // Samplers
    SamplerH create_sampler(const SamplerInfo &info);
    Sampler &get_sampler(SamplerH H);
    void destroy_sampler(SamplerH H);

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
void destroy_sampler_internal(API &api, Sampler &sampler);
void destroy_program_internal(API &api, GraphicsProgram &program);
void destroy_program_internal(API &api, ComputeProgram &program);
void destroy_shader_internal(API &api, Shader &shader);

inline ImageAccess get_src_image_access(ImageUsage usage)
{
    ImageAccess access;
    switch (usage)
    {
        case ImageUsage::GraphicsShaderRead:
        {
            access.stage  = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
            access.access = 0;
            access.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        break;
        case ImageUsage::GraphicsShaderReadWrite:
        {
            access.stage  = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            access.access = VK_ACCESS_SHADER_WRITE_BIT;
            access.layout = VK_IMAGE_LAYOUT_GENERAL;
        }
        break;
        case ImageUsage::ComputeShaderRead:
        {
            access.stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            access.access = 0;
            access.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        break;
        case ImageUsage::ComputeShaderReadWrite:
        {
            access.stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            access.access = VK_ACCESS_SHADER_WRITE_BIT;
            access.layout = VK_IMAGE_LAYOUT_GENERAL;
        }
        break;
        case ImageUsage::TransferDst:
        {
            access.stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
            access.access = VK_ACCESS_TRANSFER_WRITE_BIT;
            access.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        }
        break;
        case ImageUsage::TransferSrc:
        {
            access.stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
            access.access = 0;
            access.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        }
        break;
        case ImageUsage::ColorAttachment:
        {
            access.stage  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            access.access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            access.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
        break;
        case ImageUsage::DepthAttachment:
        {
            access.stage  = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            access.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            access.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }
        break;
        case ImageUsage::Present:
        {
            access.stage  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            access.access = 0;
            access.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }
        break;
        case ImageUsage::None:
        {
            access.stage  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            access.access = 0;
            access.layout = VK_IMAGE_LAYOUT_UNDEFINED;
        }
        break;
    };
    return access;
}

inline ImageAccess get_dst_image_access(ImageUsage usage)
{
    ImageAccess access;
    switch (usage)
    {
        case ImageUsage::GraphicsShaderRead:
        {
            access.stage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            access.access = VK_ACCESS_SHADER_READ_BIT;
            access.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        break;
        case ImageUsage::GraphicsShaderReadWrite:
        {
            access.stage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            access.access = VK_ACCESS_SHADER_WRITE_BIT;
            access.layout = VK_IMAGE_LAYOUT_GENERAL;
        }
        break;
        case ImageUsage::ComputeShaderRead:
        {
            access.stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            access.access = VK_ACCESS_SHADER_READ_BIT;
            access.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        break;
        case ImageUsage::ComputeShaderReadWrite:
        {
            access.stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            access.access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            access.layout = VK_IMAGE_LAYOUT_GENERAL;
        }
        break;
        case ImageUsage::TransferDst:
        {
            access.stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
            access.access = VK_ACCESS_TRANSFER_WRITE_BIT;
            access.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        }
        break;
        case ImageUsage::TransferSrc:
        {
            access.stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
            access.access = VK_ACCESS_TRANSFER_READ_BIT;
            access.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        }
        break;
        case ImageUsage::ColorAttachment:
        {
            access.stage  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            access.access = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            access.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
        break;
        case ImageUsage::DepthAttachment:
        {
            access.stage  = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            access.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            access.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }
        break;
        case ImageUsage::Present:
        {
            access.stage  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            access.access = 0;
            access.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }
        break;
        case ImageUsage::None:
        {
            access.stage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            access.access = 0;
            access.layout = VK_IMAGE_LAYOUT_UNDEFINED;
        }
        break;
    };
    return access;
}

inline bool is_image_barrier_needed(ImageUsage src, ImageUsage dst)
{
    return !(src == ImageUsage::GraphicsShaderRead && dst == ImageUsage::GraphicsShaderRead);
}

inline VkImageMemoryBarrier get_image_barrier(VkImage image, const ImageAccess &src, const ImageAccess &dst, const VkImageSubresourceRange &range)
{
    VkImageMemoryBarrier b = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.oldLayout            = src.layout;
    b.newLayout            = dst.layout;
    b.srcAccessMask        = src.access;
    b.dstAccessMask        = dst.access;
    b.image                = image;
    b.subresourceRange     = range;
    return b;
}

inline VkImageMemoryBarrier get_image_barrier(const Image &image, const ImageAccess &src, const ImageAccess &dst)
{
    return get_image_barrier(image.vkhandle, src, dst, image.full_range);
}

inline VkPrimitiveTopology vk_topology_from_enum(PrimitiveTopology topology)
{
    switch (topology)
    {
    case PrimitiveTopology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case PrimitiveTopology::PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    }

    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

inline usize vk_format_size(VkFormat format)
{
    constexpr usize SFLOAT_SIZE = 4;
    switch (format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM: return 4 * 1;
    case VK_FORMAT_R32G32_SFLOAT: return 2 * SFLOAT_SIZE;
    default: break;
    }
    assert(false);
    return 4;
}

inline VkBufferUsageFlagBits vk_buffer_usage_from_descriptor_type(VkDescriptorType desc_type)
{
    switch (desc_type)
    {
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    default: break;
    }
    assert(false);
    return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
}

inline bool is_uint(VkFormat format)
{
    switch (format)
    {
        case VK_FORMAT_R8_UINT:
        case VK_FORMAT_R8G8_UINT:
        case VK_FORMAT_R8G8B8_UINT:
        case VK_FORMAT_B8G8R8_UINT:
        case VK_FORMAT_R8G8B8A8_UINT:
        case VK_FORMAT_B8G8R8A8_UINT:
        case VK_FORMAT_A8B8G8R8_UINT_PACK32:
        case VK_FORMAT_A2R10G10B10_UINT_PACK32:
        case VK_FORMAT_A2B10G10R10_UINT_PACK32:
        case VK_FORMAT_R16_UINT:
        case VK_FORMAT_R16G16_UINT:
        case VK_FORMAT_R16G16B16_UINT:
        case VK_FORMAT_R16G16B16A16_UINT:
        case VK_FORMAT_R32_UINT:
        case VK_FORMAT_R32G32_UINT:
        case VK_FORMAT_R32G32B32_UINT:
        case VK_FORMAT_R32G32B32A32_UINT:
        case VK_FORMAT_R64_UINT:
        case VK_FORMAT_R64G64_UINT:
        case VK_FORMAT_R64G64B64_UINT:
        case VK_FORMAT_R64G64B64A64_UINT:
        case VK_FORMAT_S8_UINT:
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return true;
        default:
        {
        }
    }
    return false;
}

inline bool is_sint(VkFormat format)
{
    switch (format)
    {
        case VK_FORMAT_R8_SINT:
        case VK_FORMAT_R8G8_SINT:
        case VK_FORMAT_R8G8B8_SINT:
        case VK_FORMAT_B8G8R8_SINT:
        case VK_FORMAT_R8G8B8A8_SINT:
        case VK_FORMAT_B8G8R8A8_SINT:
        case VK_FORMAT_A8B8G8R8_SINT_PACK32:
        case VK_FORMAT_A2R10G10B10_SINT_PACK32:
        case VK_FORMAT_A2B10G10R10_SINT_PACK32:
        case VK_FORMAT_R16_SINT:
        case VK_FORMAT_R16G16_SINT:
        case VK_FORMAT_R16G16B16_SINT:
        case VK_FORMAT_R16G16B16A16_SINT:
        case VK_FORMAT_R32_SINT:
        case VK_FORMAT_R32G32_SINT:
        case VK_FORMAT_R32G32B32_SINT:
        case VK_FORMAT_R32G32B32A32_SINT:
        case VK_FORMAT_R64_SINT:
        case VK_FORMAT_R64G64_SINT:
        case VK_FORMAT_R64G64B64_SINT:
        case VK_FORMAT_R64G64B64A64_SINT:
            return true;
        default:
        {
        }
    }
    return false;
}
} // namespace vulkan
} // namespace my_app
