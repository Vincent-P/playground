#pragma once

#include <exo/maths/numerics.h>
#include <exo/option.h>
#include <exo/handle.h>
#include <exo/collections/dynamic_array.h>

#include "render/vulkan/descriptor_set.h"
#include "render/vulkan/operators.h"

#include <string>
#include <vulkan/vulkan.h>

namespace vulkan
{
inline constexpr usize MAX_ATTACHMENTS         = 4; // Maximum number of attachments (color + depth) in a framebuffer
inline constexpr usize MAX_RENDERPASS          = 4; // Maximum number of renderpass (combination of load operator) per framebuffer
inline constexpr usize MAX_SHADER_DESCRIPTORS  = 4; // Maximum number of descriptors in a shader descriptor set (usually just one uniform buffer)
inline constexpr usize MAX_DYNAMIC_DESCRIPTORS = 4; // Maximum number of total dynamic descriptors (in all descriptor sets)
inline constexpr usize MAX_RENDER_STATES       = 4; // Maximum number of render state per pipeline

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
    DynamicArray<LoadOp, MAX_ATTACHMENTS> load_ops;
};

struct FramebufferFormat
{
    i32 width = 0;
    i32 height = 0;
    u32 layer_count = 1;
    DynamicArray<VkFormat, MAX_ATTACHMENTS> attachments_format;
    Option<VkFormat> depth_format;
    bool operator==(const FramebufferFormat &) const = default;
};

struct  Framebuffer
{
    VkFramebuffer vkhandle = VK_NULL_HANDLE;
    FramebufferFormat format;
    DynamicArray<Handle<Image>, MAX_ATTACHMENTS> color_attachments;
    Handle<Image> depth_attachment;

    DynamicArray<RenderPass, MAX_RENDERPASS> renderpasses;

    bool operator==(const Framebuffer &) const = default;
};

// Everything needed to build a pipeline except render state which is a separate struct
struct GraphicsState
{
    Handle<Shader> vertex_shader;
    Handle<Shader> fragment_shader;
    FramebufferFormat attachments_format;
    DynamicArray<DescriptorType, MAX_SHADER_DESCRIPTORS> descriptors;
};

struct GraphicsProgram
{
    std::string name;
    // state to compile the pipeline
    GraphicsState graphics_state;
    DynamicArray<RenderState, MAX_RENDER_STATES> render_states;

    // pipeline
    VkPipelineLayout pipeline_layout;
    DynamicArray<VkPipeline, MAX_RENDER_STATES> pipelines;
    VkPipelineCache cache;
    VkRenderPass renderpass;

    // data binded to the program
    DescriptorSet descriptor_set;
};

struct ComputeState
{
    Handle<Shader> shader;
    DynamicArray<DescriptorType, MAX_SHADER_DESCRIPTORS> descriptors;
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
    ASSERT(false);
    return VK_ATTACHMENT_LOAD_OP_MAX_ENUM;
}

}
