#pragma once
#include <exo/cross/file_watcher.h>
#include <exo/cross/window.h>

#include "inputs.h"
#include "ui.h"
#include "scene.h"

#include "render/render_world.h"

struct ScopeStack;
struct AssetManager;
struct Renderer;

class App
{
  public:
    static App *create(ScopeStack &scope);
    ~App();

    void run();

  private:
    void camera_update();
    void display_ui();

    cross::Window *window;
    AssetManager  *asset_manager;
    Renderer      *renderer;

    UI::Context ui;
    Inputs inputs;

    RenderWorld render_world;

    Scene scene;

    cross::FileWatcher watcher;
    cross::Watch shaders_watch;

    bool is_minimized;
};
