#include "app.hpp"
#include "camera.hpp"
#include "timer.hpp"
#include "window.hpp"
#if defined(ENABLE_IMGUI)
#include <imgui.h>
#endif

namespace my_app
{

static float CAMERA_SPEED      = 20.0f;
static float MOUSE_SENSITIVITY = 0.5f;

static constexpr float3 UP    = float3(0, 1, 0);
static constexpr float3 FRONT = float3(0, 0, 1);

Camera Camera::create(float3 position)
{
    Camera camera{};
    camera.position = position;
    camera.front    = FRONT;
    camera.up       = UP;
    camera.update_view();
    return camera;
}

InputCamera InputCamera::create(Window &window, TimerData& timer, UI::Context &ui, float3 position)
{
    InputCamera camera{};
    camera._internal = Camera::create(position);
    camera.p_window  = &window;
    camera.p_timer  = &timer;
    camera.p_ui      = &ui;
    return camera;
}

void InputCamera::on_mouse_movement(double xpos, double ypos)
{
    bool pressed = glfwGetKey(p_window->get_handle(), GLFW_KEY_LEFT_ALT);
    if (!pressed) {
	return;
    }

    if (last_xpos == 0.0) {
	last_xpos = xpos;
    }
    if (last_ypos == 0.0) {
	last_ypos = ypos;
    }

    float yaw_increment   = (float(xpos) - last_xpos) * MOUSE_SENSITIVITY;
    float pitch_increment = (float(ypos) - last_ypos) * MOUSE_SENSITIVITY;

    _internal.yaw += yaw_increment;
    _internal.pitch += pitch_increment;

    if (_internal.pitch > 89.0f) {
	_internal.pitch = 89.0f;
    }
    if (_internal.pitch < -89.0f) {
	_internal.pitch = -89.0f;
    }

    _internal.update_view();

    last_xpos = xpos;
    last_ypos = ypos;
}

void InputCamera::update()
{
    float delta_t = p_timer->get_delta_time();

    int forward = 0;
    int right   = 0;

    if (glfwGetKey(p_window->get_handle(), GLFW_KEY_W)) {
	forward++;
    }
    if (glfwGetKey(p_window->get_handle(), GLFW_KEY_A)) {
	right--;
    }
    if (glfwGetKey(p_window->get_handle(), GLFW_KEY_S)) {
	forward--;
    }
    if (glfwGetKey(p_window->get_handle(), GLFW_KEY_D)) {
	right++;
    }

    if (forward) {
	_internal.position += (CAMERA_SPEED * float(forward) * delta_t) * _internal.front;
    }

    if (right) {
	auto camera_right = glm::normalize(glm::cross(_internal.front, _internal.up));
	_internal.position += (CAMERA_SPEED * float(right) * delta_t) * camera_right;
    }

    if (forward || right) {
	_internal.update_view();
    }

#if defined(ENABLE_IMGUI)
    if (p_ui->begin_window("Camera", true))
    {
        ImGui::SliderFloat("Camera speed", &CAMERA_SPEED, 0.1f, 250.f);
        ImGui::SliderFloat("Mouse sensitivity", &MOUSE_SENSITIVITY, 0.f, 1.f);
        ImGui::SliderFloat3("position", &_internal.position[0], 1.1f, 100000.0f);
        ImGui::SliderFloat3("up", &_internal.up[0], -180.0f, 180.0f);
        ImGui::SliderFloat3("front", &_internal.front[0], -180.0f, 180.0f);
        p_ui->end_window();
    }
#endif

}

float4x4 Camera::update_view()
{
    // make quaternion from euler angles
    rotation = float3(glm::radians(pitch), glm::radians(-yaw), glm::radians(roll));

    front = rotation * FRONT;
    up    = rotation * UP;
    view  = glm::lookAt(position, position + front, up);

    return view;
}

float4x4 Camera::perspective(float fov, float aspect_ratio, float near_plane, float far_plane)
{
    (void)(far_plane);
    float f = 1.0f / tan(glm::radians(fov) / 2.0f);
    projection = glm::mat4(
        f / aspect_ratio, 0.0f,       0.0f,  0.0f,
                    0.0f,   -f,       0.0f,  0.0f,
                    0.0f, 0.0f,       0.0f, -1.0f,
                    0.0f, 0.0f, near_plane,  0.0f);

    return projection;
}

float4x4 Camera::ortho_square(float size, float near_plane, float far_plane)
{
    projection = glm::ortho(-size, size, -size, size, near_plane, far_plane);
    projection[1][1] *= -1;
    return projection;
}

} // namespace my_app
