#include "types.h"
#include "globals.h"

layout(set = 3, binding = 0) uniform Options {
    u32 first_instance;
};

layout(set = 3, binding = 1) buffer PositionsBuffer {
    float4 positions[];
};

layout(set = 3, binding = 2) buffer InstancesBuffer {
    RenderInstance render_instances[];
};

void main()
{
    RenderInstance instance = render_instances[first_instance + gl_InstanceIndex];
    float4 position = instance.transform * float4(positions[gl_VertexIndex].xyz, 1.0);
    gl_Position = globals.camera_projection * globals.camera_view * position;
}
