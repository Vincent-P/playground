#pragma once

#include <exo/types.h>
#include <exo/collections/vector.h>
#include <exo/option.h>
#include <exo/handle.h>

#include "render/vulkan/descriptor_set.h"
#include "render/vulkan/operators.h"

#include <string>
#include <vulkan/vulkan.h>

namespace vulkan
{

struct Shader
{
    std::string filename;
    VkShaderModule vkhandle;
    Vec<u8> bytecode;
    bool operator==(const Shader &other) const = default;
};

enum struct PrimitiveTopology
{
    TriangleList,
    PointList
};

struct DepthState
{
    Option<VkCompareOp> test = std::nullopt;
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

struct RenderState
{
    DepthState depth;
    RasterizationState rasterization;
    InputAssemblyState input_assembly;
    bool alpha_blending = false;

    bool operator==(const RenderState &) const = default;
};


struct LoadOp
{
    enum struct Type
    {
        Load,
        Clear,
        Ignore
    };

    Type type;
    VkClearValue color;

    static inline LoadOp load()
    {
        LoadOp load_op;
        load_op.type = Type::Load;
        load_op.color = {};
        return load_op;
    }

    static inline LoadOp clear(VkClearValue color)
    {
        LoadOp load_op;
        load_op.type = Type::Clear;
        load_op.color = color;
        return load_op;
    }

    static inline LoadOp ignore()
    {
        LoadOp load_op;
        load_op.type = Type::Ignore;
        load_op.color = {};
        return load_op;
    }

    inline bool operator==(const LoadOp& other) const
    {
        return this->type == other.type && this->color == other.color;
    }
};

struct RenderPass
{
    VkRenderPass vkhandle;
    Vec<LoadOp> load_ops;
};

struct FramebufferFormat
{
    i32 width = 0;
    i32 height = 0;
    u32 layer_count = 1;
    Vec<VkFormat> attachments_format;
    Option<VkFormat> depth_format;
    bool operator==(const FramebufferFormat &) const = default;
};

struct  Framebuffer
{
    VkFramebuffer vkhandle = VK_NULL_HANDLE;
    FramebufferFormat format;
    Vec<Handle<Image>> color_attachments;
    Handle<Image> depth_attachment;

    Vec<RenderPass> renderpasses;

    bool operator==(const Framebuffer &) const = default;
};

// Everything needed to build a pipeline except render state which is a separate struct
struct GraphicsState
{
    Handle<Shader> vertex_shader;
    Handle<Shader> fragment_shader;
    FramebufferFormat attachments_format;
    Vec<DescriptorType> descriptors;
};

struct GraphicsProgram
{
    std::string name;
    // state to compile the pipeline
    GraphicsState graphics_state;
    Vec<RenderState> render_states;

    // pipeline
    VkPipelineLayout pipeline_layout;
    Vec<VkPipeline> pipelines;
    VkPipelineCache cache;
    VkRenderPass renderpass;

    // data binded to the program
    DescriptorSet descriptor_set;
};

struct ComputeState
{
    Handle<Shader> shader;
    Vec<DescriptorType> descriptors;
};

struct ComputeProgram
{
    std::string name;
    ComputeState state;
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    DescriptorSet descriptor_set;
};

// -- Utils

inline VkPrimitiveTopology to_vk(PrimitiveTopology topology)
{
    switch (topology)
    {
    case PrimitiveTopology::TriangleList:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case PrimitiveTopology::PointList:
        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    }
    return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
}

inline VkAttachmentLoadOp to_vk(LoadOp op)
{
    switch (op.type)
    {
    case LoadOp::Type::Load: return VK_ATTACHMENT_LOAD_OP_LOAD;
    case LoadOp::Type::Clear: return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case LoadOp::Type::Ignore: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
    assert(false);
    return VK_ATTACHMENT_LOAD_OP_MAX_ENUM;
}

}
