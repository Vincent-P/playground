#include "gameplay/components/camera_component.h"

#include "camera.h"
#include "gameplay/loading_context.h"

void CameraComponent::look_at(float3 eye, float3 at, float3 up)
{
    view = camera::look_at(eye, at, up, &view_inverse);
}

void CameraComponent::set_perspective(float aspect_ratio)
{
    projection = camera::infinite_perspective(fov, aspect_ratio, near_plane, &projection_inverse);
}
