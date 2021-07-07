#pragma once
#include "asset_manager.hpp"
#include "base/types.hpp"
#include "platform/file_watcher.hpp"
#include "inputs.hpp"
#include "platform/window.hpp"
#include "render/renderer.hpp"
#include "ui.hpp"
#include "scene.hpp"

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
