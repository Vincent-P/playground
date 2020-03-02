#pragma once
#include "renderer/renderer.hpp"
#include "window.hpp"
#include "camera.hpp"
#include "timer.hpp"

namespace my_app
{

class App
{
  public:
    App();
    ~App();

    NO_COPY_NO_MOVE(App)

    void run();

  private:
    void draw_fps();
    void camera_update();
    void update();

    Window window;
    InputCamera camera;
    Renderer renderer;
    TimerData timer;
};

} // namespace my_app
