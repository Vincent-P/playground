#pragma once
#include <exo/prelude.h>

#include <exo/os/file_watcher.h>
#include <engine/inputs.h>
#include <engine/scene.h>
#include <engine/render/render_world.h>

struct ScopeStack;
struct AssetManager;
struct Renderer;
namespace exo {struct Platform;}
namespace exo {struct Window;}
namespace exo {struct ScopeStack;}

class App
{
  public:
    static App *create(exo::ScopeStack &scope);
    ~App();

    void run();

  private:
    void camera_update();
    void display_ui();

	exo::Platform *platform;
    exo::Window *window;
    AssetManager  *asset_manager;
    Renderer      *renderer;

    Inputs inputs;

    RenderWorld render_world;

    Scene scene;

    exo::FileWatcher watcher;
    exo::Watch shaders_watch;

    bool is_minimized;
};
