#pragma once
#include "renderer.h"
#include <cross/file_watcher.h>
#include <engine/render_world.h>
#include <engine/scene.h>
#include <gameplay/inputs.h>
#include <painter/font.h>
#include <ui/docking.h>

#include "custom_ui.h"
#include "ui.h"

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
	void display_ui(double dt);

	cross::Platform        *platform;
	cross::Window          *window;
	AssetManager           *asset_manager;
	Renderer                renderer;
	ui::Ui                  ui;
	Font                    ui_font;
	Painter                *painter;
	docking::Docking        docking;
	custom_ui::FpsHistogram histogram;

	Inputs inputs;

	RenderWorld render_world;

	Scene scene;

	cross::FileWatcher watcher;

	bool is_minimized;
};
