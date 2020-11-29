#include "types.h"
#include "globals.h"

layout (location = 0) in float4 i_near;
layout (location = 1) in float4 i_far;
layout (location = 0) out float4 o_color;

float4 grid(float3 world_pos, float line_width, float grid_spacing)
{
    float2 coords = world_pos.xz / grid_spacing;
    float2 wrapped = fract(coords - 0.5) - 0.5;
    float2 range = abs(wrapped);


    // more precise than deritatives = fwidth(coords)
    float4 deltas = float4(dFdx(coords), dFdy(coords));
    float2 deritatives = sqrt(float2(
                      dot(deltas.xz, deltas.xz),
                      dot(deltas.yw, deltas.yw)
                      ));

    float2 pixel_range = range / deritatives;
    float line_weight = clamp(min(pixel_range.x, pixel_range.y) - (line_width - 1), 0, 1);

    float3 line_color = float3(0.8);
    float line_opacity = 1.0 - min(line_weight, 1.0);

    float half_space = grid_spacing * line_width * min(deritatives.y, 1);
    // x axis
    if(-half_space < world_pos.z && world_pos.z < half_space) {
        line_color.rgb = float3(1.0, 0.22, 0.33);
    }

    half_space = grid_spacing * line_width * min(deritatives.x, 1);
    // z axis
    if(-half_space < world_pos.x && world_pos.x < half_space) {
        line_color.rgb = float3(0.2, 0.57, 0.96);
    }

    return float4(line_color * line_opacity, line_opacity);
}

void main()
{
    // discard points that are not on the ground

    // y = near.y + t * (far.y - near.y)
    float t = -i_near.y / (i_far.y - i_near.y);
    if (t < 0) {
        discard;
    }

    float4 world_pos = i_near + t * (i_far - i_near);

    // one grid for meters and one for 0.1 meters
    o_color = 0.5 * grid(world_pos.xyz, 1.5, 1.0)
        /* + 0.5 * grid(world_pos.xyz, 1.0, 0.1) */
        ;

    // Project the point of the grid to write the depth correctly (is it needed?)
    float4 projected_pos = global.camera_proj * global.camera_view * world_pos;
    gl_FragDepth = projected_pos.z / projected_pos.w;

    // Fade the grid based on the distance from the camera
    float3 camera_to_point = world_pos.xyz - global.camera_pos;
    float squared_d = camera_to_point.x * camera_to_point.x + camera_to_point.y * camera_to_point.y + camera_to_point.z * camera_to_point.z;

    // Use 1/d^2 attenuation
    const float SCALE = 100;
    float fade = SCALE / (squared_d + 1);
    o_color.a *= clamp(fade, 0, 1);

    if (o_color.a < 0.01) {
        discard;
    }
}
