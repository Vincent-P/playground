#include <imgui.h>
#include "camera.hpp"
#include "window.hpp"

namespace my_app
{

static float CAMERA_SPEED = 0.5f;
static float MOUSE_SENSITIVITY = 0.5f;

Camera Camera::create(float3 position)
{
    Camera camera{};
    camera.position = position;
    camera.front = float3(0, 0, 1);
    camera.up = float3(0, 1, 0);
    return camera;
}

InputCamera InputCamera::create(Window& window, float3 position)
{
    InputCamera camera{};
    camera._internal = Camera::create(position);
    camera.p_window = &window;
    return camera;
}

void InputCamera::on_mouse_movement(double xpos, double ypos)
{
    float yaw_increment = (float(xpos) - last_xpos) * MOUSE_SENSITIVITY;
    float pitch_increment = (float(ypos) - last_ypos) * MOUSE_SENSITIVITY;

    yaw += yaw_increment;
    pitch += pitch_increment;

    if (pitch > 89.0f) {
        pitch = 89.0f;
    }
    if (pitch < -89.0f) {
        pitch = -89.0f;
    }

    float3 angles = float3(glm::radians(pitch), glm::radians(-yaw), 0);

    glm::quat orientation = angles;
    _internal.front = orientation * glm::vec3(0, 0, 1);
    _internal.up = orientation * glm::vec3(0, 1, 0);

    last_xpos = xpos;
    last_ypos = ypos;
}

void InputCamera::update()
{
    static constexpr auto delta_t = 16.f;


    ImGui::Begin("Camera");
    ImGui::SliderFloat("Camera speed", &CAMERA_SPEED, 0.f, 1.f);
    ImGui::SliderFloat("Mouse sensitivity", &MOUSE_SENSITIVITY, 0.f, 1.f);
    ImGui::End();


    int forward = 0;
    int right = 0;

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
}

}
