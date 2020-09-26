#include "camera.hpp"

#include "app.hpp"
#include "timer.hpp"
#include "platform/window.hpp"
#if defined(ENABLE_IMGUI)
#    include <imgui/imgui.h>
#endif

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

    void InputCamera::create(InputCamera& camera, window::Window &window, TimerData &timer, float3 position)
{
    Camera::create(camera._internal, position);
    camera.p_window  = &window;
    camera.p_timer   = &timer;
}

void InputCamera::on_mouse_movement(double xpos, double ypos)
{
    float delta_t = p_timer->get_delta_time();

    switch (state)
    {
        case States::Idle:
        {
            break;
        }
        case States::Move:
        {
            float up    = ypos - dragged_mouse_start_pos.y;
            float right = xpos - dragged_mouse_start_pos.x;

            if (up != 0.0f || right != 0.0f)
            {
                auto camera_plane_forward = glm::normalize(float3(_internal.front.x, 0.0f, _internal.front.z));
                auto camera_right         = glm::cross(_internal.up, _internal.front);
                auto camera_plane_right   = glm::normalize(float3(camera_right.x, 0.0f, camera_right.z));

                target += (CAMERA_MOVE_SPEED * delta_t) * right * camera_plane_right;
                target += (CAMERA_MOVE_SPEED * delta_t) * up     * camera_plane_forward;

                view_dirty = true;

                // reset drag?
                dragged_mouse_start_pos = float2(xpos, ypos);
            }

            break;
        }
        case States::Orbit:
        {
            float up    = ypos - dragged_mouse_start_pos.y;
            float right = dragged_mouse_start_pos.x - xpos;

            if (up != 0.0f || right != 0.0f)
            {
                theta += (CAMERA_ROTATE_SPEED * delta_t) * right;

                constexpr auto low = -90.0f;
                constexpr auto high = 0.0f;
                if (low <= phi && phi < high)
                {
                    phi += (CAMERA_ROTATE_SPEED * delta_t) * up;
                    phi = glm::clamp(phi, low, high - 1.0f);
                }

                view_dirty = true;

                // reset drag?
                dragged_mouse_start_pos = float2(xpos, ypos);
            }

            break;
        }
        case States::Zoom:
        {
            break;
        }
    }
}

void InputCamera::on_mouse_scroll(double /*xoffset*/, double yoffset)
{
    float delta_t = p_timer->get_delta_time();
    switch (state)
    {
        case States::Idle:
        {
            target.y += (CAMERA_SCROLL_SPEED * delta_t) * yoffset;
            view_dirty = true;
            break;
        }
        case States::Move:
        {
            break;
        }
        case States::Orbit:
        {

            break;
        }
        case States::Zoom:
        {
            r += (CAMERA_SCROLL_SPEED * delta_t) * -yoffset;
            r = glm::max(r, 0.1f);
            view_dirty = true;
            break;
        }
    }
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
        ImGui::SliderFloat("move speed", &CAMERA_MOVE_SPEED, 0.1f, 250.f);
        ImGui::SliderFloat("rotate speed", &CAMERA_ROTATE_SPEED, 0.1f, 250.f);
        ImGui::SliderFloat("scroll speed", &CAMERA_SCROLL_SPEED, 0.1f, 250.f);
        ImGui::SliderFloat3("position", &_internal.position[0], 1.1f, 100000.0f);

        ImGui::SliderFloat3("up", &_internal.up[0], -180.0f, 180.0f);
        ImGui::SliderFloat3("front", &_internal.front[0], -180.0f, 180.0f);

        ImGui::SliderFloat3("target", &target[0], -180.0f, 180.0f);
        ImGui::SliderFloat("spherical r", &r, 0.1f, 180.0f);
        ImGui::SliderFloat("spherical theta", &theta, -180.0f, 180.0f);
        ImGui::SliderFloat("spherical phi", &phi, -180.0f, 180.0f);

        ui.end_window();
    }
}

void InputCamera::update()
{
    auto &window = *p_window;

    bool alt_pressed = window.is_key_pressed(window::VirtualKey::Alt);
    bool lmb_pressed = window.is_mouse_button_pressed(window::MouseButton::Left);
    bool rmb_pressed = window.is_mouse_button_pressed(window::MouseButton::Right);

    switch (state)
    {
        case States::Idle:
        {
            if (alt_pressed && lmb_pressed)
            {
                state = States::Move;

                dragged_mouse_start_pos = window.get_mouse_position();
            }
            else if (alt_pressed && rmb_pressed)
            {
                state = States::Orbit;

                dragged_mouse_start_pos = window.get_mouse_position();
            }
            else if (alt_pressed)
            {
                state = States::Zoom;
            }
            break;
        }
        case States::Move:
        {
            if (!alt_pressed || !lmb_pressed)
            {
                state = States::Idle;
            }
            break;
        }
        case States::Orbit:
        {
            if (!alt_pressed || !rmb_pressed)
            {
                state = States::Idle;
            }
            break;
        }
        case States::Zoom:
        {
            if (!alt_pressed || lmb_pressed || rmb_pressed)
            {
                state = States::Idle;
            }
            break;
        }
    }

    if (view_dirty)
    {
        float theta_rad     = glm::radians(theta);
        float phi_rad     = glm::radians(phi);

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

        _internal.view = glm::lookAt(_internal.position, target, _internal.up);
    }
}

float4x4 Camera::update_view()
{
    // make quaternion from euler angles
    rotation = float3(glm::radians(pitch), glm::radians(-yaw), glm::radians(roll));

    front = rotation * FRONT;
    up    = rotation * UP;
    view  = glm::lookAt(position, position + front, UP);

    return view;
}

float4x4 Camera::perspective(float fov, float aspect_ratio, float near_plane, float far_plane)
{
    float f    = 1.0f / std::tan(glm::radians(fov) / 2.0f);

    float far_on_range = far_plane / (near_plane - far_plane);

    auto projection = float4x4(f / aspect_ratio, 0.0f,                        0.0f,  0.0f,
                               0.0f,            -f,                           0.0f,  0.0f,
                               0.0f,             0.0f,       - far_on_range - 1.0f, -1.0f,
                               0.0f,             0.0f, - near_plane * far_on_range,  0.0f);

    return projection;
}
} // namespace my_app
