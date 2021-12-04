#pragma shader_stage(compute)

#include "types.h"
#include "constants.h"
#include "globals.h"

layout(set = SHADER_SET, binding = 0) uniform Options {
    u32 predicate_descriptor;
    u32 draw_arguments_descriptor;
};

#define PREDICATE_BUFFER global_buffers_uint[predicate_descriptor].data
#define DRAW_COUNT global_buffers_draw_arguments[draw_arguments_descriptor].draw_count
#define DRAW_ARGUMENTS_BUFFER global_buffers_draw_arguments[draw_arguments_descriptor].arguments

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;
void main()
{
    uint local_idx  = gl_LocalInvocationIndex;
    uint global_idx = gl_GlobalInvocationID.x;

    if (global_idx >= DRAW_COUNT)
    {
        return;
    }

    PREDICATE_BUFFER[global_idx] = u32(
        DRAW_ARGUMENTS_BUFFER[global_idx].instance_count != 0
        && DRAW_ARGUMENTS_BUFFER[global_idx].vertex_count != 0
    );
}
