#pragma once
#include <exo/cross/file_watcher.h>
#include <exo/cross/window.h>

#include "inputs.h"
#include "ui.h"
#include "scene.h"

#include "assets/asset_manager.h"

#include "render/render_world.h"
#include "render/renderer.h"

class App
{
  public:
    App();
    ~App();

    void run();

  private:
    void camera_update();
    void display_ui();

    UI::Context ui;
    cross::Window window;
    Inputs inputs;
    AssetManager asset_manager;

    RenderWorld render_world;
    Renderer renderer;

    Scene scene;

    cross::FileWatcher watcher;
    cross::Watch shaders_watch;

    bool is_minimized;
};
