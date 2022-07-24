#pragma once
#include <render/simple_renderer.h>

struct RenderWorld;
struct Renderer
{
	SimpleRenderer base;

	static Renderer create(u64 window_handle);
	void            draw(const RenderWorld &world);
};
