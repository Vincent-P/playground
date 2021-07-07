#pragma shader_stage(compute)

#include "types.h"
#include "constants.h"
#include "globals.h"
#include "csm.h"

layout (set = 1, binding = 0) uniform sampler2D reduction_input;

layout(set = 1, binding = 1) buffer GpuMatrices {
    float4 depth_slices[2];
    CascadeMatrix matrices[4];
};

float4x4 look_at(float3 eye, float3 at, float3 up)
{
    float3 z_axis = normalize(at - eye);
    float3 x_axis = normalize(cross(z_axis, up));
    float3 y_axis = cross(x_axis, z_axis);

    float4x4 result = float4x4(
        x_axis.x  ,  x_axis.y,   x_axis.z,   -dot(eye,x_axis),
        y_axis.x  ,  y_axis.y,   y_axis.z,   -dot(eye,y_axis),
        -z_axis.x ,  -z_axis.y,  -z_axis.z,  dot(eye, z_axis),
        0.0f      ,  0.0f,       0.0f,       1.0f
    );

    // I like to declare matrices as in maths but opengl wants to take the values for each columns...
    return transpose(result);
}

float4x4 ortho(float3 min_clip, float3 max_clip)
{
    float x_range = max_clip.x - min_clip.x;
    float y_range = max_clip.y - min_clip.y;
    float z_range = max_clip.z - min_clip.z;

    float4x4 projection = float4x4(
        2.0/x_range, 0.0,         0.0,          -1.0*(max_clip.x+min_clip.x)/x_range,
        0.0,         2.0/y_range, 0.0,          -1.0*(max_clip.y+min_clip.y)/y_range,
        0.0,         0.0,         -1.0/z_range, 1.0*(max_clip.z+min_clip.z)/y_range,
        0.0,         0.0,         0.0,          1.0
    );

    // I like to declare matrices as in maths but opengl wants to take the values for each columns...
    return transpose(projection);
}

// TODO: set 4 threads to compute cascades in parallel
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main()
{
    float2 max_min_linear = texelFetch(reduction_input, int2(0, 0), LOD0).rg;

    float4 slices[2] = {float4(0.0), float4(0.0)};

    float n = max_min_linear[1] * (global.camera_far - global.camera_near) + global.camera_near;
    float f = max_min_linear[0] * (global.camera_far - global.camera_near) + global.camera_near;

    for (uint i = 0; i < 5; i++)
    {
        slices[i/4][i%4] = n * pow(f / n, i * 0.25);
        depth_slices[i/4][i%4] = - global.camera_proj[2].z  + global.camera_proj[3].z / slices[i/4][i%4];
    }

    for (uint i = 0; i < 4; i++)
    {
        float last_split = depth_slices[i/4][i%4];
        uint i_next = i+1;
        float split = depth_slices[i_next/4][i_next%4];

        float4 frustum_corners[8] =
        {
            float4(-1.0,  1.0, last_split,  1.0),
            float4( 1.0,  1.0, last_split,  1.0),
            float4( 1.0, -1.0, last_split,  1.0),
            float4(-1.0, -1.0, last_split,  1.0),
            float4(-1.0,  1.0,      split,  1.0),
            float4( 1.0,  1.0,      split,  1.0),
            float4( 1.0, -1.0,      split,  1.0),
            float4(-1.0, -1.0,      split,  1.0)
        };

        // project frustum corners into world space
        for (uint i = 0; i < 8; i++)
        {
            frustum_corners[i] = global.camera_inv_view_proj * frustum_corners[i];
            frustum_corners[i] /= frustum_corners[i].w;
        }

        // get frustum center
        float3 center = float3(0.0);
        for (uint i = 0; i < 8; i++)
        {
            center += frustum_corners[i].xyz;
        }
        center = center / 8;

        // get the radius of the frustum
        float radius = 0.0;
        for (uint i = 0; i < 8; i++)
        {
            float d = distance(center, frustum_corners[i].xyz);
            radius = max(radius, d);
        }
        radius = ceil(radius * 16.0) / 16.0;

        float3 light_dir = global.sun_direction;

        float3 max = float3(radius);
        float3 min = -max;

        matrices[i].view = look_at(center + light_dir * radius, center, float3(0, 1, 0));

        // reverse depth
        min.z            = (max.z - min.z);
        max.z            = 0.0;

        matrices[i].proj = ortho(min, max);
    }
}
