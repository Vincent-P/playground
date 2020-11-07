#include "globals.h"

layout (location = 0) out float4 o_near;
layout (location = 1) out float4 o_far;


vec3 plane_vertices[6] = vec3[](
    vec3(1, 1, 0),
    vec3(-1, -1, 0),
    vec3(-1, 1, 0),
    vec3(-1, -1, 0),
    vec3(1, 1, 0),
    vec3(1, -1, 0)
);

void main()
{
    float3 vertex = plane_vertices[gl_VertexIndex];

    vertex.z = 1.0;
    o_near = global.camera_inv_view_proj * float4(vertex, 1.0);

    vertex.z = 0.0;
    o_far = global.camera_inv_view_proj * float4(vertex, 1.0);

    o_near /= o_near.w;
    o_far  /= o_far.w;

    gl_Position = float4(vertex, 1.0);
}
