#include "camera.h"

#include <cmath>

namespace camera
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
    auto n = near_plane;
    auto f = far_plane;

    float focal_length = 1.0f / std::tan(to_radians(fov) / 2.0f); // = 2n / (height)
    // aspect_ratio = width/height
    float x  =  focal_length / aspect_ratio; // (2n/height)*(height/width) => 2n/width
    float y  = -focal_length; // -2n/height

    assert((n - f) != 0.0f);
    float n_on_f_minus_n = n / (f - n);

    float A = n_on_f_minus_n;
    float B = f * A;

    // bad things will happen in the inverse
    assert(B != 0.0f);
    assert(x != 0.0f);
    assert(y != 0.0f);

    // clang-format off
    float4x4 projection{{
        x,    0.0f, 0.0f,  0.0f,
        0.0f, y,    0.0f,  0.0f,
        0.0f, 0.0f,    A,     B,
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
            0.0f, 0.0f, 1/B, A/B,
        });
        // clang-format on
    }

    return projection;
}

float4x4 infinite_perspective(float fov, float aspect_ratio, float near_plane, float4x4 *inverse)
{
    auto n = near_plane;

    float focal_length = 1.0f / std::tan(to_radians(fov) / 2.0f); // = 2n / (height)
    // aspect_ratio = width/height
    float x  =  focal_length / aspect_ratio; // (2n/height)*(height/width) => 2n/width
    float y  = -focal_length; // -2n/height

    // bad things will happen in the inverse
    assert(x != 0.0f);
    assert(y != 0.0f);
    assert(n != 0.0f);

    // clang-format off
    float4x4 projection{{
        x,    0.0f, 0.0f,  0.0f,
        0.0f, y,    0.0f,  0.0f,
        0.0f, 0.0f,    0,     n,
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
            0.0f, 0.0f, 1/n,  0.0f,
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
} // namespace camera
