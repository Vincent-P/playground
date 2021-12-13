#pragma once
#include <exo/os/file_watcher.h>

#include <engine/inputs.h>
#include <engine/scene.h>
#include <engine/render/render_world.h>

struct ScopeStack;
struct AssetManager;
struct Renderer;
namespace exo::os {struct Window;}

class App
{
  public:
    static App *create(ScopeStack &scope);
    ~App();

    void run();

  private:
    void camera_update();
    void display_ui();

    os::Window *window;
    AssetManager  *asset_manager;
    Renderer      *renderer;

    Inputs inputs;

    RenderWorld render_world;

    Scene scene;

    os::FileWatcher watcher;
    os::Watch shaders_watch;

    bool is_minimized;
};
