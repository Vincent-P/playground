#include "camera.hpp"
#include "window.hpp"
#if defined(ENABLE_IMGUI)
#include <imgui.h>
#endif

namespace my_app
{

static float CAMERA_SPEED      = 0.02f;
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

InputCamera InputCamera::create(Window &window, float3 position)
{
    InputCamera camera{};
    camera._internal = Camera::create(position);
    camera.p_window  = &window;
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
    static constexpr auto delta_t = 16.f;

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
    ImGui::Begin("Camera");
    ImGui::SliderFloat("Camera speed", &CAMERA_SPEED, 0.f, 0.25f);
    ImGui::SliderFloat("Mouse sensitivity", &MOUSE_SENSITIVITY, 0.f, 1.f);
    ImGui::SliderFloat3("position", &_internal.position[0], -180.0f, 180.0f);
    ImGui::SliderFloat3("up", &_internal.up[0], -180.0f, 180.0f);
    ImGui::SliderFloat3("front", &_internal.front[0], -180.0f, 180.0f);
    ImGui::End();
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
    projection = glm::perspective(glm::radians(fov), aspect_ratio, near_plane, far_plane);
    projection[1][1] *= -1;
    return projection;
}
float4x4 Camera::ortho_square(float size, float near_plane, float far_plane)
{
    projection = glm::ortho(-size, size, -size, size, near_plane, far_plane);
    projection[1][1] *= -1;
    return projection;
}

} // namespace my_app
