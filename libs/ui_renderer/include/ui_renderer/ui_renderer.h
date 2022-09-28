#pragma once
#include <exo/collections/handle.h>
#include <exo/maths/vectors.h>

namespace vulkan
{
struct Image;
struct GraphicsProgram;
struct Device;
}; // namespace vulkan
struct RenderGraph;
struct TextureDesc;
struct Painter;
struct GraphicPass;

struct UiRenderer
{
	Handle<vulkan::GraphicsProgram> ui_program  = {};
	Handle<vulkan::Image>           glyph_atlas = {};

	static UiRenderer create(vulkan::Device &device, int2 atlas_resolution);
};

GraphicPass &register_graph(RenderGraph &graph, UiRenderer &renderer, Painter *painter, Handle<TextureDesc> output);
