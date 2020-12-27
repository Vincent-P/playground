#extension GL_ARB_shader_draw_parameters : require

#include "types.h"
#include "globals.h"
#define PBR_NO_NORMALS
#include "pbr.h"

layout(set = 1, binding = 0) buffer readonly Vertices {
    Vertex vertices[];
};

layout(set = 1, binding = 1) buffer readonly Materials {
    Material materials[];
};

layout(set = 1, binding = 2) buffer readonly DrawDatas {
    DrawData draw_datas[];
};

layout (set = 1, binding = 3) buffer readonly Transforms {
    float4x4 transforms[]; // max nodes
};

layout (location = 0) out float3 out_position;
layout (location = 1) out float3 out_normal;
layout (location = 2) out float2 out_uv0;
layout (location = 3) out float2 out_uv1;
layout (location = 4) out float4 out_color0;
layout (location = 5) out float4 out_joint0;
layout (location = 6) out float4 out_weight0;
layout (location = 7) out flat int o_drawid;

void main()
{
    DrawData current_draw = draw_datas[gl_DrawIDARB];
    float4x4 transform = transforms[current_draw.transform_idx];
    Vertex vertex = vertices[gl_VertexIndex + current_draw.vertex_idx];

    float4 world_pos = transform * float4(vertex.position, 1.0);

    out_normal = normalize(transpose(inverse(float3x3(transform))) * vertex.normal);
    out_position = world_pos.xyz;
    out_uv0 = vertex.uv0;
    out_uv1 = vertex.uv1;
    out_color0 = vertex.color0;
    out_joint0 = vertex.joint0;
    out_weight0 = vertex.weight0;
    gl_Position = get_jittered_projection(global.camera_proj, global.jitter_offset) * global.camera_view * world_pos;
    o_drawid = gl_DrawIDARB;
}
