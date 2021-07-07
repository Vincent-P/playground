#include "constants.h"
#include "globals.h"
#include "pbr.h"
#include "maths.h"

// 0 is vertices

layout(set = 1, binding = 1) buffer readonly Materials {
    Material materials[];
};

layout(set = 1, binding = 2) buffer readonly DrawDatas {
    DrawData draw_datas[];
};

// 3 is transforms

layout (location = 0) in float3 i_world_pos;
layout (location = 1) in float3 i_normal;
layout (location = 2) in float2 i_uv0;
layout (location = 3) in float2 i_uv1;
layout (location = 4) in float4 i_color0;
layout (location = 5) in float4 i_joint0;
layout (location = 6) in float4 i_weight0;
layout (location = 7) in flat int i_drawid;

layout (location = 0) out float4 o_color;
void main()
{
    DrawData current_draw = draw_datas[i_drawid];
    Material material = materials[current_draw.material_idx];

    float3 normal = get_normal(material, i_world_pos, i_normal, i_uv0);
    float4 base_color = get_base_color(material, i_uv0) * i_color0;
    float2 metallic_roughness = get_metallic_roughness(material, i_uv0);

    // PBR
    float3 albedo = base_color.rgb;
    float3 N = i_normal;
    float3 V = normalize(global.camera_pos - i_world_pos);
    float metallic = metallic_roughness.r;
    float roughness = metallic_roughness.g;

    float3 L = global.sun_direction; // point towards sun
    float3 radiance = global.sun_illuminance; // wrong unit
    float NdotL = max(dot(N, L), 0.0);

    float visibility = 1.0;

    /// --- Lighting

    float3 lighting = visibility * BRDF(albedo, N, V, metallic, roughness, L) * radiance * NdotL;
    lighting += albedo * PI * (radiance / 100);
    o_color = float4(lighting, 1.0);
}
