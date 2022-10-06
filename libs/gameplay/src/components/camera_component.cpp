#include "gameplay/components/camera_component.h"

#include <exo/macros/assert.h>

#include "gameplay/loading_context.h"

#include <cmath> // std::tan

namespace details::camera
{
float4x4 look_at(float3 eye, float3 at, float3 up, float4x4 *inverse)
{
	float3 z_axis = normalize(at - eye);
	float3 x_axis = normalize(cross(z_axis, up));
	float3 y_axis = cross(x_axis, z_axis);

	// clang-format off
    float4x4 result = float4x4({
        x_axis.x  ,  x_axis.y,   x_axis.z,   -dot(eye,x_axis),
        y_axis.x  ,  y_axis.y,   y_axis.z,   -dot(eye,y_axis),
        -z_axis.x ,  -z_axis.y,  -z_axis.z,  dot(eye, z_axis),
        0.0f      ,  0.0f,       0.0f,       1.0f,
    });
	// clang-format on

	if (inverse) {
		// clang-format off
        *inverse = float4x4({
                x_axis.x ,  y_axis.x,  -z_axis.x,  eye.x,
                x_axis.y ,  y_axis.y,  -z_axis.y,  eye.y,
                x_axis.z ,  y_axis.z,  -z_axis.z,  eye.z,
                0.0f     ,  0.0f,      0.0f,       1.0f,
        });
		// clang-format on
	}

	return result;
}

float4x4 ortho(float3 min_clip, float3 max_clip, float4x4 *inverse)
{
	(void)(inverse);
	float x_range = max_clip.x - min_clip.x;
	float y_range = max_clip.y - min_clip.y;
	float z_range = max_clip.z - min_clip.z;

	ASSERT(x_range != 0.0f);
	ASSERT(y_range != 0.0f);
	ASSERT(z_range != 0.0f);

	// clang-format off
    float4x4 projection = float4x4({
        2.0f/x_range, 0.0f,         0.0f,          -1.0f*(max_clip.x+min_clip.x)/x_range,
        0.0f,         2.0f/y_range, 0.0f,          -1.0f*(max_clip.y+min_clip.y)/y_range,
        0.0f,         0.0f,         -1.0f/z_range, 1.0f*(max_clip.z+min_clip.z)/y_range,
        0.0f,         0.0f,         0.0f,          1.0f,
    });
	// clang-format on

	ASSERT(!inverse);

	return projection;
}
} // namespace details::camera

void CameraComponent::look_at(float3 eye, float3 at, float3 up)
{
	view = details::camera::look_at(eye, at, up, &view_inverse);
}
