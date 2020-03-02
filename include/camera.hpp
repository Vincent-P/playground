#pragma once

#include <glm/gtc/quaternion.hpp>
#include "types.hpp"

namespace my_app
{

class Window;

struct Camera
{
    float3 position;
    float3 front;
    float3 up;

    glm::quat rotation;
    float4x4 view;
    float4x4 projection;

    static Camera create(float3 position);
};


struct InputCamera
{
    Camera _internal;
    Window* p_window;
    float pitch;
    float yaw;
    float roll;
    double last_xpos;
    double last_ypos;

    static InputCamera create(Window& window, float3 position);
    void on_mouse_movement(double xpos, double ypos);
    void update();
};

}
