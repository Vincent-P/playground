#pragma once
#include <exo/collections/dynamic_array.h>
#include <exo/collections/handle.h>
#include <exo/option.h>

#include "render/vulkan/framebuffer.h"
#include "render/vulkan/shader.h"

#include <string>
#include <volk.h>

namespace vulkan
{
struct Image;

enum struct PrimitiveTopology
{
	TriangleList,
	PointList
};

struct DepthState
{
	Option<VkCompareOp> test         = std::nullopt;
	bool                enable_write = false;
	float               bias         = 0.0f;

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
	PrimitiveTopology topology                                     = PrimitiveTopology::TriangleList;
	bool              operator==(const InputAssemblyState &) const = default;
};

struct RenderState
{
	DepthState         depth;
	RasterizationState rasterization;
	InputAssemblyState input_assembly;
	bool               alpha_blending = false;

	bool operator==(const RenderState &) const = default;
};

// Everything needed to build a pipeline except render state which is a separate struct
struct GraphicsState
{
	Handle<Shader>    vertex_shader;
	Handle<Shader>    fragment_shader;
	FramebufferFormat attachments_format;
};

struct GraphicsProgram
{
	std::string name;
	// state to compile the pipeline
	GraphicsState                                     graphics_state;
	exo::DynamicArray<RenderState, MAX_RENDER_STATES> render_states;

	// pipeline
	exo::DynamicArray<VkPipeline, MAX_RENDER_STATES> pipelines;
	VkRenderPass                                     renderpass;
};

struct ComputeState
{
	Handle<Shader> shader;
};

struct ComputeProgram
{
	std::string  name;
	ComputeState state;
	VkPipeline   pipeline;
};

// -- Utils

inline VkPrimitiveTopology to_vk(PrimitiveTopology topology)
{
	switch (topology) {
	case PrimitiveTopology::TriangleList:
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	case PrimitiveTopology::PointList:
		return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	}
	return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
}

} // namespace vulkan
