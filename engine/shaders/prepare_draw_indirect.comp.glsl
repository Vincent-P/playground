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

layout(set = 1, binding = 0) buffer readonly DrawCommands {
    u32 draw_count;
    DrawIndirectCommand commands[];
};

layout(set = 1, binding = 1) buffer readonly DrawDatas {
    DrawData draw_datas[];
};

layout(set = 1, binding = 2) buffer readonly PrimitivesBuffer {
    Primitive primitives[];
};

layout (set = 1, binding = 3) buffer readonly Transforms {
    float4x4 transforms[]; // max nodes
};

layout (set = 1, binding = 4) buffer writeonly Visibility {
    u32 visibility[];
};

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;
void main()
{
    uint local_idx = gl_LocalInvocationIndex;
    uint3 global_idx = gl_GlobalInvocationID;
    uint3 group_idx = gl_WorkGroupID;

    uint draw_idx = global_idx.x;
    if (draw_idx >= draw_count) {
        return;
    }
    DrawData current_draw = draw_datas[draw_idx];
    float4x4 transform = transforms[current_draw.transform_idx];
    Primitive primitive = primitives[current_draw.primitive_idx];

    float3 a_min = primitive.aab_min;
    float3 a_max = primitive.aab_max;

    float radius = length(a_max - a_min) / 2;
    float3 center = (a_min + a_max) / 2;

    float4 center_e = global.camera_view * transform * float4(center, 1);

    float4x4 projection_rows = transpose(global.camera_proj);

    // Left clipping plane is
    // -w <= x
    // 0 <= w + x
    // 0 <= row_4 v + row_1 v
    // 0 <= (row_4 + row_1) v

    // Right clipping plane is
    // x <= w
    // 0 <= w - x
    // 0 <= row_4 v - row_1 v
    // 0 <= (row_4 - row_1) v

    // between is -w <= x <= w
    // abs(x) <= w
    // 0 <= w - abs(x)
    // 0 <= row_4 v - abs(row_1 v)
    // 0 <= row_4 v - abs(row_1) abs(v)

    float4 left_plane   = projection_rows[3] + projection_rows[0];
    float4 right_plane  = projection_rows[3] - projection_rows[0];
    float4 top_plane    = projection_rows[3] + projection_rows[1];
    float4 bottom_plane = projection_rows[3] - projection_rows[1];

    bool outside_depth = (-global.camera_near - (center_e.z - radius)) < 0;

    bool is_visible =
           radius > dot(left_plane,    center_e)
        && radius > dot(right_plane,  center_e)
        && radius > dot(top_plane,    center_e)
        && radius > dot(bottom_plane, center_e)
        && !outside_depth
        ;

    visibility[draw_idx] = u32(is_visible);
}
