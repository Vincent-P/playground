#pragma once
#include "render/simple_renderer.h"
#include "ui_renderer/ui_renderer.h"

#include "mesh_renderer.h"

struct RenderWorld;
struct AssetManager;
struct Painter;

struct SrgbPass
{
	Handle<vulkan::ComputeProgram> program;
};

struct DrawInput
{
	const RenderWorld *world               = nullptr;
	float2             world_viewport_size = float2(-1.0f);
	Painter           *painter             = nullptr;
};

struct DrawResult
{
	u32 glyph_atlas_index;
	u32 scene_viewport_index;
};

struct Renderer
{
	SimpleRenderer base;
	MeshRenderer   mesh_renderer;
	UiRenderer     ui_renderer;
	SrgbPass       srgb_pass;

	AssetManager *asset_manager = nullptr;

    static Renderer create(u64 display_handle, u64 window_handle, AssetManager *asset_manager);

	DrawResult draw(DrawInput input);
};
