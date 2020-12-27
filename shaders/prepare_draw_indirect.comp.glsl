#pragma shader_stage(compute)

#include "types.h"
#include "constants.h"
#include "globals.h"
#define PBR_NO_NORMALS
#include "pbr.h"
#include "maths.h"

struct DrawIndirectCommand
{
    u32 index_count;
    u32 instance_count;
    u32 first_index;
    i32 vertex_offset;
    u32 first_instance;
};

layout(set = 1, binding = 0) buffer DrawCommands {
    u32 draw_count;
    DrawIndirectCommand commands[];
};

layout(set = 1, binding = 1) buffer readonly DrawDatas {
    DrawData draw_datas[];
};

layout(set = 1, binding = 2) buffer PrimitivesBuffer {
    Primitive primitives[];
};

layout (set = 1, binding = 3) buffer readonly Transforms {
    float4x4 transforms[]; // max nodes
};


bool inside_plane(float3 point, float3 plane_normal)
{
    return dot(point, plane_normal) > 0;
}

layout(local_size_x = 16, local_size_y = 1, local_size_z = 1) in;
void main()
{
    uint local_idx = gl_LocalInvocationIndex;
    uint3 global_idx = gl_GlobalInvocationID;
    uint3 group_idx = gl_WorkGroupID;

    uint draw_idx = global_idx.x;
    if (draw_idx > draw_count) {
        return;
    }
    DrawData current_draw = draw_datas[draw_idx];
    float4x4 transform = transforms[current_draw.transform_idx];
    Primitive primitive = primitives[current_draw.primitive_idx];

    float3 a_min = primitive.aab_min;
    float3 a_max = primitive.aab_max;
    float3 corners[8] = {
        float3(a_min.x, a_min.y, a_min.z),
        float3(a_min.x, a_min.y, a_max.z),
        float3(a_min.x, a_max.y, a_min.z),
        float3(a_min.x, a_max.y, a_max.z),
        float3(a_max.x, a_min.y, a_min.z),
        float3(a_max.x, a_min.y, a_max.z),
        float3(a_max.x, a_max.y, a_min.z),
        float3(a_max.x, a_max.y, a_max.z)
        };

    bool is_visible = true;

    bool is_outside1 = true;
    bool is_outside2 = true;
    bool is_outside3 = true;
    bool is_outside4 = true;
    bool is_outside5 = true;
    bool is_outside6 = true;
    for (uint i = 0; i < 8; i++)
    {
        float4 projected = global.camera_proj * global.camera_view * transform * float4(corners[i], 1.0);
        is_outside1 = is_outside1 && (-projected.w > projected.x);
        is_outside2 = is_outside2 && ( projected.x > projected.w);
        is_outside3 = is_outside3 && (-projected.w > projected.y);
        is_outside4 = is_outside4 && ( projected.y > projected.w);
        is_outside5 = is_outside4 && (           0 > projected.z);
        is_outside6 = is_outside4 && ( projected.z > projected.w);
    }

    bool is_outside = is_outside1 || is_outside2 || is_outside3 || is_outside4 || is_outside5 || is_outside6;
    commands[draw_idx].instance_count = u32(!is_outside);
    commands[draw_idx].instance_count = 1;
}
