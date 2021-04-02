#include "types.h"
#include "globals.h"

layout(set = 1, binding = 0) uniform Options {
    u32 padd0;
    u32 padd1;
};

layout(set = 1, binding = 1) buffer glTFVertexBufferType {
    Vertex vertices[];
};
void main()
{
    Vertex vertex = vertices[gl_VertexIndex];
    float3 position = vertex.position;
    const float scale = 0.1;
    float4 world_pos = globals.camera_projection * globals.camera_view * float4(scale * position, 1.0);
    gl_Position = world_pos;
}
