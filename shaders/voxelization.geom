#version 450

layout (triangles) in;
layout (triangle_strip, max_vertices=3) out;

layout (location = 0) in vec3 inWorldPos[];
layout (location = 1) in vec3 inNormal[];
layout (location = 2) in vec2 inUV0[];
layout (location = 3) in vec2 inUV1[];

layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outUV0;
layout (location = 3) out vec2 outUV1;
layout (location = 4) out vec3 outVoxelPos;

layout(set = 1, binding = 0) uniform VoxelOptions {
    vec3 center;
    float size;
    uint res;
} debug_options;

layout(set = 1, binding = 1) uniform ProjectionMatrices {
    mat4 matrices[3];
} voxel_projections;

void main(void)
{
    vec3 face_normal = abs(inNormal[0] + inNormal[1] + inNormal[2]);

    uint maxi = face_normal[1] > face_normal[0] ? 1 : 0;
    maxi = face_normal[2] > face_normal[maxi] ? 2 : maxi;

    for (uint i = 0; i < gl_in.length(); i++)
    {
        // project based on dominant normal
        gl_Position = voxel_projections.matrices[maxi] * vec4(inWorldPos[i], 1);

        // voxel space
        vec3 voxel_center = floor(debug_options.center);
        vec3 voxel_pos = (inWorldPos[i] - voxel_center) / debug_options.size;
        outVoxelPos = voxel_pos;

        outWorldPos = inWorldPos[i];
        outNormal = inNormal[i];
        outUV0 = inUV0[i];
        outUV1 = inUV1[i];
        EmitVertex();
    }
    EndPrimitive();
}
