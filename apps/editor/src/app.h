#pragma once
#include "assets/asset_manager.h"
#include "cross/file_watcher.h"
#include "cross/jobmanager.h"
#include "cross/window.h"
#include "custom_ui.h"
#include "engine/render_world.h"
#include "engine/scene.h"
#include "exo/maths/vectors.h"
#include "gameplay/inputs.h"
#include "painter/font.h"
#include "painter/painter.h"
#include "renderer.h"
#include "ui/docking.h"
#include "ui/ui.h"

struct ScopeStack;
struct AssetManager;
struct Renderer;
namespace exo
{
struct ScopeStack;
}

class App
{
public:
	App(exo::ScopeStack &scope);
	~App();

	void run();

private:
	void display_ui(double dt);

	cross::JobManager              jobmanager;
	std::unique_ptr<cross::Window> window;
	AssetManager                   asset_manager;
	Renderer                       renderer;

	// -- UI
	Font                    ui_font;
	Painter                 painter;
	ui::Ui                  ui;
	docking::Docking        docking;
	custom_ui::FpsHistogram histogram;
	// 3d viewport
	float2 viewport_size;
	u32    viewport_texture_index;

	Inputs inputs;

	RenderWorld render_world;

	Scene scene;

	cross::FileWatcher watcher;

	bool is_minimized;
};
