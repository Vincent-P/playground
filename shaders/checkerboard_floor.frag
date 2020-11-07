#include "types.h"
#include "globals.h"

layout (location = 0) in float4 i_near;
layout (location = 1) in float4 i_far;
layout (location = 0) out float4 o_color;

vec4 grid(float3 world_pos, float scale)
{
    float2 coord = world_pos.xz * scale; // use the scale variable to set the distance between the lines
    float2 derivative = fwidth(coord);
    float2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    float min_z = min(derivative.y, 1);
    float min_x = min(derivative.x, 1);

    const float GRAY = 0.3;
    float4 color = float4(GRAY, GRAY, GRAY, 1.0 - min(line, 1.0));


    float epsilon = 1/scale;
    // z axis
    if(world_pos.x > -epsilon * min_x && world_pos.x < epsilon * min_x) {
        color.rgb = float3(0.2, 0.57, 0.96);
    }

    // x axis
    if(world_pos.z > -epsilon * min_z && world_pos.z < epsilon * min_z) {
        color.rgb = float3(1.0, 0.22, 0.33);
    }

    return color;
}

void main()
{
    // y = near.y + t * (far.y - near.y)
    float t = -i_near.y / (i_far.y - i_near.y);

    float4 world_pos = i_near + t * (i_far - i_near);

    o_color = grid(world_pos.xyz, 1.0) /* + grid(world_pos.xyz, 10.0) */;

    float3 camera_to_point = world_pos.xyz - global.camera_pos;
    camera_to_point /= 100;
    float squared_d = camera_to_point.x * camera_to_point.x + camera_to_point.y * camera_to_point.y + camera_to_point.z * camera_to_point.z;

    const float MAX_DIST = 200;
    const float SQUARED_MAX_DIST = MAX_DIST * MAX_DIST;

    float fade = 1 - clamp(squared_d, 0, SQUARED_MAX_DIST) / SQUARED_MAX_DIST;
    fade = 1 / (squared_d + 1);
    o_color.a *= fade;

    if (t < 0 || o_color.a < 0.01) {
        discard;
    }
}
