#pragma once
#include <exo/cross/file_watcher.h>

#include <engine/inputs.h>
#include <engine/scene.h>
#include <engine/render/render_world.h>

struct ScopeStack;
struct AssetManager;
struct Renderer;
namespace cross {struct Window;}

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

    Inputs inputs;

    RenderWorld render_world;

    Scene scene;

    cross::FileWatcher watcher;
    cross::Watch shaders_watch;

    bool is_minimized;
};
