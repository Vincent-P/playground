#pragma once
#include "renderer.h"
#include <cross/file_watcher.h>
#include <engine/render_world.h>
#include <engine/scene.h>
#include <gameplay/inputs.h>

struct ScopeStack;
struct AssetManager;
struct Renderer;
namespace cross
{
struct Platform;
}
namespace cross
{
struct Window;
}
namespace exo
{
struct ScopeStack;
}

class App
{
public:
	static App *create(exo::ScopeStack &scope);
	~App();

	void run();

private:
	void camera_update();

	cross::Platform *platform;
	cross::Window   *window;
	AssetManager    *asset_manager;
	Renderer         renderer;

	Inputs inputs;

	RenderWorld render_world;

	Scene scene;

	cross::FileWatcher watcher;

	bool is_minimized;
};
