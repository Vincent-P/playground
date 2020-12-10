#pragma once
#include "base/types.hpp"
#include "ecs.hpp"
#include "file_watcher.hpp"
#include "inputs.hpp"
#include "platform/window.hpp"
#include "render/renderer.hpp"
#include "timer.hpp"
#include "ui.hpp"

namespace my_app
{

class App
{
  public:
    App();
    ~App();

    void run();

  private:
    void camera_update();
    void update();
    void display_ui();

    UI::Context ui;
    platform::Window window;
    Inputs inputs;
    ECS::EntityId main_camera;
    Renderer renderer;
    TimerData timer;
    ECS::World ecs;

    FileWatcher watcher;
    Watch shaders_watch;

    bool is_minimized;
};

} // namespace my_app
