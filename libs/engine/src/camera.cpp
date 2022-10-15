#include "engine/camera.h"

#include <exo/macros/assert.h>

#include <cmath> // std::tan

namespace camera
{
float4x4 look_at(float3 eye, float3 at, float3 up, float4x4 *inverse)
{
	const float3 z_axis = normalize(at - eye);
	const float3 x_axis = normalize(cross(z_axis, up));
	const float3 y_axis = cross(x_axis, z_axis);

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

float4x4 perspective(float fov, float aspect_ratio, float near_plane, float far_plane, float4x4 *inverse)
{
	auto n = near_plane;
	auto f = far_plane;

	const float focal_length = 1.0f / std::tan(exo::to_radians(fov) / 2.0f); // = 2n / (height)
	// aspect_ratio = width/height
	const float x = focal_length / aspect_ratio; // (2n/height)*(height/width) => 2n/width
	const float y = -focal_length;               // -2n/height

	ASSERT((n - f) != 0.0f);
	const float n_on_f_minus_n = n / (f - n);

	const float A = n_on_f_minus_n;
	const float B = f * A;

	// bad things will happen in the inverse
	ASSERT(B != 0.0f);
	ASSERT(x != 0.0f);
	ASSERT(y != 0.0f);

	// clang-format off
    float4x4 projection{{
        x,    0.0f, 0.0f,  0.0f,
        0.0f, y,    0.0f,  0.0f,
        0.0f, 0.0f,    A,     B,
        0.0f, 0.0f, -1.0f, 0.0f,
    }};
	// clang-format on

	if (inverse) {
		// clang-format off
        *inverse = float4x4({
            1/x,  0.0f, 0.0f, 0.0f,
            0.0f, 1/y,  0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, -1.0f,
            0.0f, 0.0f, 1/B, A/B,
        });
		// clang-format on
	}

	return projection;
}

float4x4 infinite_perspective(float fov, float aspect_ratio, float near_plane, float4x4 *inverse)
{
	auto n = near_plane;

	const float focal_length = 1.0f / std::tan(exo::to_radians(fov) / 2.0f); // = 2n / (height)
	// aspect_ratio = width/height
	const float x = focal_length / aspect_ratio; // (2n/height)*(height/width) => 2n/width
	const float y = -focal_length;               // -2n/height

	// bad things will happen in the inverse
	ASSERT(x != 0.0f);
	ASSERT(y != 0.0f);
	ASSERT(n != 0.0f);

	// clang-format off
    float4x4 projection{{
        x,    0.0f, 0.0f,  0.0f,
        0.0f, y,    0.0f,  0.0f,
        0.0f, 0.0f,    0,     n,
        0.0f, 0.0f, -1.0f, 0.0f,
    }};
	// clang-format on

	if (inverse) {
		// clang-format off
        *inverse = float4x4({
            1/x,  0.0f, 0.0f, 0.0f,
            0.0f, 1/y,  0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, -1.0f,
            0.0f, 0.0f, 1/n,  0.0f,
        });
		// clang-format on
	}

	return projection;
}

float4x4 ortho(float3 min_clip, float3 max_clip, float4x4 *inverse)
{
	(void)(inverse);
	const float x_range = max_clip.x - min_clip.x;
	const float y_range = max_clip.y - min_clip.y;
	const float z_range = max_clip.z - min_clip.z;

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
} // namespace camera
