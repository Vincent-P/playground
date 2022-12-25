#pragma once
#include "cross/file_watcher.h"
#include "render/render_graph/builtins.h"
#include "render/render_graph/graph.h"
#include "render/ring_buffer.h"
#include "render/vulkan/context.h"
#include "render/vulkan/device.h"

constexpr usize FRAME_QUEUE_LENGTH = 2;

struct SimpleRenderer
{
	vulkan::Context context;
	vulkan::Device device;
	vulkan::WorkPool workpools[FRAME_QUEUE_LENGTH];
	RingBuffer uniform_buffer;
	RingBuffer dynamic_vertex_buffer;
	RingBuffer dynamic_index_buffer;
	RingBuffer upload_buffer;
	RenderGraph render_graph;
	builtins::SwapchainPass swapchain_node;
	usize frame_count = 0;
	float time = 0.0;
	cross::FileWatcher shader_watcher;

	static SimpleRenderer create(u64 display_handle, u64 window_handle);
	void destroy();

	void start_frame();
	void render(Handle<TextureDesc> output, float dt);
	void end_frame();
	const vulkan::Surface &surface();

private:
	void reload_shaders();
};
