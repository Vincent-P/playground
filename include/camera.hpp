#pragma once

#include "base/types.hpp"
#include <glm/gtc/quaternion.hpp>

namespace window{struct Window;}
namespace my_app
{

namespace UI
{
    struct Context;
};

class TimerData;

struct Camera
{
    float3 position;
    float3 front;
    float3 up;

    float pitch;
    float yaw;
    float roll;
    glm::quat rotation;

    float4x4 view;
    float4x4 projection;
    float near_plane;
    float far_plane;

    float4x4 update_view();
    [[nodiscard]] inline float4x4 get_view() const { return view; }
    [[nodiscard]] inline float4x4 get_projection() const { return projection; }

    static float4x4 perspective(float fov, float aspect_ratio, float near_plane, float far_plane);

    static void create(Camera &camera, float3 position);
};

struct InputCamera
{
    enum struct States
    {
        Idle,
        Move,
        Orbit,
        Zoom
    };

    States state;
    float2 dragged_mouse_start_pos;
    bool view_dirty = true;

    float3 target = {0.0f, 3.0f, 0.0f};

    // spherical coordinates: radius r, azymuthal angle theta, polar angle phi
    float r = 6.0f;
    float theta = -78.0f;
    float phi = -65.0f;

    Camera _internal;
    window::Window *p_window;
    UI::Context *p_ui;
    TimerData *p_timer;

    double last_xpos;
    double last_ypos;

    static void create(InputCamera& camera, window::Window &window, TimerData& timer, float3 position);
    void on_mouse_movement(double xpos, double ypos);
    void on_mouse_scroll(double xoffset, double yoffset);
    void update();
    void display_ui(UI::Context &ui);
};

} // namespace my_app
