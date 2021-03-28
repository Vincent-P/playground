#include "types.h"
#include "globals.h"

layout(buffer_reference) buffer TransformsBufferType {
    float4x4 transforms[];
};

layout(set = 1, binding = 0) uniform Options {
    TransformsBufferType transforms_ptr;
};

void main()
{
    glTFVertex vertex = globals.vertex_buffer_ptr.vertices[gl_VertexIndex];
    float4 world_pos = globals.camera_projection * globals.camera_view * float4(vertex.position, 1.0);
    gl_Position = world_pos;
}
