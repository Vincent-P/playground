#pragma once
#include "asset_manager.h"
#include <exo/types.h>
#include <cross/file_watcher.h>
#include "inputs.h"
#include <cross/window.h>
#include "render/renderer.h"
#include "ui.h"
#include "scene.h"

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
    platform::Window window;
    Inputs inputs;
    AssetManager asset_manager;
    Renderer renderer;
    Scene scene;

    platform::FileWatcher watcher;
    platform::Watch shaders_watch;

    bool is_minimized;
};
