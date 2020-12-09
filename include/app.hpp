#pragma once
#include "base/types.hpp"
#include "ecs.hpp"
#include "file_watcher.hpp"
#include "platform/window.hpp"
#include "render/renderer.hpp"
#include "timer.hpp"
#include "inputs.hpp"
#include "ui.hpp"

namespace my_app
{

struct TransformComponent
{
    float3 position = float3(0.0);
    float3 front    = float3_FORWARD;
    float3 up       = float3_UP;
    static const char *type_name() { return "TransformComponent"; }
};

struct CameraComponent
{
    float near_plane = 0.1f;
    float far_plane  = 100.0f;
    float fov        = 60.0f;
    float4x4 view;
    float4x4 view_inverse;
    float4x4 projection;
    float4x4 projection_inverse;
    static const char *type_name() { return "CameraComponent"; }
};

struct InputCameraComponent
{
    enum struct States
    {
        Idle,
        Move,
        Orbit,
        Zoom
    };

    States state = States::Idle;
    // spherical coordinates: radius r, azymuthal angle theta, polar angle phi
    float r     = 6.0f;
    float theta = -78.0f;
    float phi   = -65.0f;
    float3 target;
    static const char *type_name() { return "InputCameraComponent"; }
};

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
