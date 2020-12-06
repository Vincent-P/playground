#include "camera.hpp"

#include "app.hpp"
#include "platform/window.hpp"
#include "inputs.hpp"
#include "timer.hpp"

#include <algorithm>
#include <cmath>
#include <imgui/imgui.h>
#include <iostream>

namespace my_app
{

static float CAMERA_MOVE_SPEED   = 5.0f;
static float CAMERA_ROTATE_SPEED = 80.0f;
static float CAMERA_SCROLL_SPEED = 80.0f;

static constexpr float3 UP    = float3(0, 1, 0);
static constexpr float3 FRONT = float3(0, 0, 1);

void Camera::create(Camera &camera, float3 position)
{
    camera.position = position;
    camera.front    = FRONT;
    camera.up       = UP;
    camera.update_view();
}

void InputCamera::create(InputCamera &camera, platform::Window &window, TimerData &timer, Inputs &inputs, float3 position)
{
    Camera::create(camera._internal, position);
    camera.p_window = &window;
    camera.p_timer  = &timer;
    camera.p_inputs = &inputs;
}

void InputCamera::display_ui(UI::Context &ui)
{
    if (ui.begin_window("Camera"))
    {
        if (state == States::Idle)
        {
            ImGui::Text("State: Idle");
        }
        else if (state == States::Move)
        {
            ImGui::Text("State: Move");
        }
        else if (state == States::Orbit)
        {
            ImGui::Text("State: Orbit");
        }
        else if (state == States::Zoom)
        {
            ImGui::Text("State: Zoom");
        }
        else
        {
            assert(false);
            ImGui::Text("State: WTF");
        }
        ImGui::SliderFloat("move speed", &CAMERA_MOVE_SPEED, 0.1f, 250.f);
        ImGui::SliderFloat("rotate speed", &CAMERA_ROTATE_SPEED, 0.1f, 250.f);
        ImGui::SliderFloat("scroll speed", &CAMERA_SCROLL_SPEED, 0.1f, 250.f);
        ImGui::SliderFloat3("position", &_internal.position.raw[0], 1.1f, 100000.0f);

        ImGui::SliderFloat3("up", &_internal.up.raw[0], -180.0f, 180.0f);
        ImGui::SliderFloat3("front", &_internal.front.raw[0], -180.0f, 180.0f);

        ImGui::SliderFloat3("target", &target.raw[0], -180.0f, 180.0f);
        ImGui::SliderFloat("spherical r", &r, 0.1f, 180.0f);
        ImGui::SliderFloat("spherical theta", &theta, -180.0f, 180.0f);
        ImGui::SliderFloat("spherical phi", &phi, -180.0f, 180.0f);

        ui.end_window();
    }
}

void InputCamera::update()
{
    auto &window = *p_window;

    float delta_t = p_timer->get_delta_time();
    bool camera_active = p_inputs->is_pressed(Action::CameraModifier);
    bool camera_move = p_inputs->is_pressed(Action::CameraMove);
    bool camera_orbit = p_inputs->is_pressed(Action::CameraOrbit);

    // state transition
    switch (state)
    {
        case States::Idle:
        {
            if (camera_active && camera_move)
            {
                state = States::Move;

                dragged_mouse_start_pos = window.get_mouse_position();
            }
            else if (camera_active && camera_orbit)
            {
                state = States::Orbit;

                dragged_mouse_start_pos = window.get_mouse_position();
            }
            else if (camera_active)
            {
                state = States::Zoom;
            }
            else
            {

                // handle inputs
                if (auto scroll = p_inputs->get_scroll_this_frame())
                {
                    target.y += (CAMERA_SCROLL_SPEED * delta_t) * scroll->y;
                    view_dirty = true;
                }

            }

            break;
        }
        case States::Move:
        {
            if (!camera_active || !camera_move)
            {
                state = States::Idle;
            }
            else
            {

                // handle inputs
                if (auto mouse_delta = p_inputs->get_mouse_delta())
                {
                    float up    = float(mouse_delta->y);
                    float right = float(mouse_delta->x);

                    auto camera_plane_forward = normalize(float3(_internal.front.x, 0.0f, _internal.front.z));
                    auto camera_right         = cross(_internal.up, _internal.front);
                    auto camera_plane_right   = normalize(float3(camera_right.x, 0.0f, camera_right.z));

                    target = target + CAMERA_MOVE_SPEED * delta_t * right * camera_plane_right;
                    target = target + CAMERA_MOVE_SPEED * delta_t * up * camera_plane_forward;

                    view_dirty = true;
                }

            }
            break;
        }
        case States::Orbit:
        {
            if (!camera_active || !camera_orbit)
            {
                state = States::Idle;
            }
            else
            {

                // handle inputs
                if (auto mouse_delta = p_inputs->get_mouse_delta())
                {
                    float up    = float(mouse_delta->y);
                    float right = -1.0f * float(mouse_delta->x);

                    theta += (CAMERA_ROTATE_SPEED * delta_t) * right;

                    constexpr auto low  = -179.0f;
                    constexpr auto high = 0.0f;
                    if (low <= phi && phi < high)
                    {
                        phi += (CAMERA_ROTATE_SPEED * delta_t) * up;
                        phi = std::clamp(phi, low, high - 1.0f);
                    }

                    view_dirty = true;
                }

            }
            break;
        }
        case States::Zoom:
        {
            if (!camera_active || camera_move || camera_orbit)
            {
                state = States::Idle;
            }
            else
            {

                // handle inputs
                if (auto scroll = p_inputs->get_scroll_this_frame())
                {
                    r += (CAMERA_SCROLL_SPEED * delta_t) * scroll->y;
                    r          = std::max(r, 0.1f);
                    view_dirty = true;
                }

            }
            break;
        }
    }

    {
        float theta_rad = to_radians(theta);
        float phi_rad   = to_radians(phi);

        float3 pos;
        pos.x = r * std::sin(phi_rad) * std::sin(theta_rad);
        pos.y = r * std::cos(phi_rad);
        pos.z = r * std::sin(phi_rad) * std::cos(theta_rad);

        _internal.front.x = -1.0f * std::sin(phi_rad) * std::sin(theta_rad);
        _internal.front.y = -1.0f * std::cos(phi_rad);
        _internal.front.z = -1.0f * std::sin(phi_rad) * std::cos(theta_rad);

        _internal.up.x = std::sin(PI / 2 + phi_rad) * std::sin(theta_rad);
        _internal.up.y = std::cos(PI / 2 + phi_rad);
        _internal.up.z = std::sin(PI / 2 + phi_rad) * std::cos(theta_rad);

        _internal.position = target + pos;

        _internal.view = Camera::look_at(_internal.position, target, _internal.up, &_internal.view_inverse);
    }
}

float4x4 Camera::update_view()
{
    // make quaternion from euler angles
    auto p = to_radians(pitch);
    auto y = to_radians(-yaw);
    auto r = to_radians(roll);

    auto y_m     = float4x4::identity();
    y_m.at(0, 0) = cos(y);
    y_m.at(0, 1) = -sin(y);
    y_m.at(1, 0) = sin(y);
    y_m.at(1, 1) = cos(y);

    auto p_m     = float4x4::identity();
    p_m.at(0, 0) = cos(p);
    p_m.at(0, 2) = sin(p);
    p_m.at(2, 0) = -sin(p);
    p_m.at(2, 2) = cos(p);

    auto r_m     = float4x4::identity();
    r_m.at(1, 1) = cos(r);
    r_m.at(1, 2) = -sin(r);
    r_m.at(2, 1) = sin(r);
    r_m.at(2, 2) = cos(r);

    auto R = y_m * p_m * r_m;

    front = (R * float4(FRONT, 1.0f)).xyz();
    up    = (R * float4(UP, 1.0f)).xyz();

    view = Camera::look_at(position, position + front, UP, &view_inverse);

    return view;
}

float4x4 Camera::look_at(float3 eye, float3 at, float3 up, float4x4 *inverse)
{
    float3 z_axis = normalize(at - eye);
    float3 x_axis = normalize(cross(z_axis, up));
    float3 y_axis = cross(x_axis, z_axis);

    float4x4 result = float4x4({
        x_axis.x,
        x_axis.y,
        x_axis.z,
        -dot(eye, x_axis),
        y_axis.x,
        y_axis.y,
        y_axis.z,
        -dot(eye, y_axis),
        -z_axis.x,
        -z_axis.y,
        -z_axis.z,
        dot(eye, z_axis),
        0.0f,
        0.0f,
        0.0f,
        1.0f,
    });

    if (inverse)
    {
        *inverse = float4x4({
            x_axis.x,
            y_axis.x,
            -z_axis.x,
            eye.x,
            x_axis.y,
            y_axis.y,
            -z_axis.y,
            eye.y,
            x_axis.z,
            y_axis.z,
            -z_axis.z,
            eye.z,
            0.0f,
            0.0f,
            0.0f,
            1.0f,
        });
    }

    return result;
}

float4x4 Camera::perspective(float fov, float aspect_ratio, float near_plane, float far_plane, float4x4 *inverse)
{
    float f            = 1.0f / std::tan(to_radians(fov) / 2.0f);
    float far_on_range = far_plane / (near_plane - far_plane);

    float x  = f / aspect_ratio;
    float y  = -f;
    float z0 = -far_on_range - 1.0f;
    float z1 = -near_plane * far_on_range;

    float4x4 projection{{
        x,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        y,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        z0,
        z1,
        0.0f,
        0.0f,
        -1.0f,
        0.0f,
    }};

    if (inverse)
    {
        *inverse = float4x4({
            1 / x,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            1 / y,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            -1.0f,
            0.0f,
            0.0f,
            1 / z1,
            z0 / z1,
        });
    }

    return projection;
}

float4x4 Camera::ortho(float3 min_clip, float3 max_clip, float4x4 *inverse)
{
    (void)(inverse);
    float x_range = max_clip.x - min_clip.x;
    float y_range = max_clip.y - min_clip.y;
    float z_range = max_clip.z - min_clip.z;

    float4x4 projection{{2.0f / x_range,
                         0.0f,
                         0.0f,
                         -1.0f * (max_clip.x + min_clip.x) / x_range,
                         0.0f,
                         2.0f / y_range,
                         0.0f,
                         -1.0f * (max_clip.y + min_clip.y) / y_range,
                         0.0f,
                         0.0f,
                         -1.0f / z_range,
                         1.0f * (max_clip.z + min_clip.z) / y_range,
                         0.0f,
                         0.0f,
                         0.0f,
                         1.0f}};

    assert(!inverse);

    return projection;
}
} // namespace my_app
