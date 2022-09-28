#pragma once
#include <render/simple_renderer.h>
#include <ui_renderer/ui_renderer.h>

#include "mesh_renderer.h"

struct RenderWorld;
struct AssetManager;
struct Painter;

struct SrgbPass
{
	Handle<vulkan::ComputeProgram> program;
};

struct Renderer
{
	SimpleRenderer base;
	MeshRenderer   mesh_renderer;
	UiRenderer     ui_renderer;
	SrgbPass       srgb_pass;

	AssetManager *asset_manager = nullptr;

	static Renderer create(u64 window_handle, AssetManager *asset_manager);
	void            draw(const RenderWorld &world, Painter *painter);
	u32             glyph_atlas_index() const;
};
