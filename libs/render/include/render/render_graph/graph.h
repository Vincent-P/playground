#pragma once
#include "exo/collections/handle.h"
#include "exo/collections/vector.h"

#include "render/render_graph/resource_registry.h"
#include "render/ring_buffer.h"

#include <functional>

namespace vulkan
{
struct Context;
struct PhysicalDevice;
struct Device;
struct GraphicsWork;
struct ComputeWork;
struct WorkPool;
} // namespace vulkan

struct RenderGraph;

enum struct PassType
{
	Graphic,
	Raw,
};

struct PassApi
{
	const vulkan::Context &context;
	vulkan::Device        &device;
	RingBuffer            &uniform_buffer;
	RingBuffer            &dynamic_vertex_buffer;
	RingBuffer            &dynamic_index_buffer;
	RingBuffer            &upload_buffer;
};

using GraphicCb = std::function<void(RenderGraph &, PassApi &, vulkan::GraphicsWork &)>;
struct GraphicPass
{
	Handle<TextureDesc> color_attachment;
	Handle<TextureDesc> depth_attachment;
	bool                clear = true;
};

using RawCb = std::function<void(RenderGraph &, PassApi &, vulkan::ComputeWork &)>;
struct RawPass
{};

struct Pass
{
	PassType type;
	union PassValue
	{
		GraphicPass graphic;
		RawPass     raw;
	} pass;
	GraphicCb execute;

	static Pass graphic(Handle<TextureDesc> color_attachment, Handle<TextureDesc> depth_attachment, GraphicCb execute)
	{
		return Pass{
			.type    = PassType::Graphic,
			.pass    = {.graphic = {.color_attachment = color_attachment, .depth_attachment = depth_attachment}},
			.execute = std::move(execute),
		};
	}

	static Pass raw(GraphicCb execute)
	{
		return Pass{
			.type    = PassType::Raw,
			.pass    = {.raw = {}},
			.execute = std::move(execute),
		};
	}
};

struct RenderGraph
{
	ResourceRegistry resources;
	Vec<Pass>        passes;
	u64              i_frame = 0;

	void         execute(PassApi api, vulkan::WorkPool &work_pool);
	void end_frame();

	GraphicPass &graphic_pass(
		Handle<TextureDesc> color_attachment, Handle<TextureDesc> depth_buffer, GraphicCb execute);
	RawPass &raw_pass(RawCb execute);

	Handle<TextureDesc> output(TextureDesc desc);
	int3                image_size(Handle<TextureDesc> desc_handle);
};
