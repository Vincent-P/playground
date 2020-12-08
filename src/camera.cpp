#include "camera.hpp"

#include <cmath>

namespace my_app::camera
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

    if (inverse)
    {
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
    float f = 1.0f / std::tan(to_radians(fov) / 2.0f);

    assert((near_plane - far_plane) != 0.0f);
    float far_on_range = far_plane / (near_plane - far_plane);

    float x  = f / aspect_ratio;
    float y  = -f;
    float z0 = -far_on_range - 1.0f;
    float z1 = -near_plane * far_on_range;

    // bad things will happen in the inverse
    assert(z1 != 0.0f);
    assert(x != 0.0f);
    assert(y != 0.0f);

    // clang-format off
    float4x4 projection{{
        x,    0.0f, 0.0f,  0.0f,
        0.0f, y,    0.0f,  0.0f,
        0.0f, 0.0f, z0,    z1,
        0.0f, 0.0f, -1.0f, 0.0f,
    }};
    // clang-format on

    if (inverse)
    {
        // clang-format off
        *inverse = float4x4({
            1/x,  0.0f, 0.0f, 0.0f,
            0.0f, 1/y,  0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, -1.0f,
            0.0f, 0.0f, 1/z1, z0/z1,
        });
        // clang-format on
    }

    return projection;
}

float4x4 ortho(float3 min_clip, float3 max_clip, float4x4 *inverse)
{
    (void)(inverse);
    float x_range = max_clip.x - min_clip.x;
    float y_range = max_clip.y - min_clip.y;
    float z_range = max_clip.z - min_clip.z;

    assert(x_range != 0.0f);
    assert(y_range != 0.0f);
    assert(z_range != 0.0f);

    // clang-format off
    float4x4 projection = float4x4({
        2.0f/x_range, 0.0f,         0.0f,          -1.0f*(max_clip.x+min_clip.x)/x_range,
        0.0f,         2.0f/y_range, 0.0f,          -1.0f*(max_clip.y+min_clip.y)/y_range,
        0.0f,         0.0f,         -1.0f/z_range, 1.0f*(max_clip.z+min_clip.z)/y_range,
        0.0f,         0.0f,         0.0f,          1.0f,
    });
    // clang-format on

    assert(!inverse);

    return projection;
}
} // namespace my_app::camera
