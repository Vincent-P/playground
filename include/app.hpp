#pragma once
#include "renderer/renderer.hpp"
#include "window.hpp"
#include "timer.hpp"

namespace my_app
{

class App
{
  public:
    App();
    ~App();

    NO_COPY_NO_MOVE(App)

    void draw_fps();
    void run();

  private:
    Window window;
    Renderer renderer;
    TimerData timer;
};

} // namespace my_app
