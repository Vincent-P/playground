#version 450

layout (triangles) in;
layout (line_strip, max_vertices = 6) out;

layout (location = 0) in vec3 inWorldPos[];
layout (location = 1) in vec3 inNormal[];
layout (location = 2) in vec2 inUV0[];
layout (location = 3) in vec2 inUV1[];

layout (location = 0) out vec3 outVoxelPos[];
layout (location = 1) out vec3 outNormal[];
layout (location = 2) out vec2 outUV0[];
layout (location = 3) out vec2 outUV1[];

#define VOXEL_DATA_CENTER vec3(0.0, 0.0, 0.0)
#define VOXEL_DATA_SIZE 100
#define VOXEL_DATA_RES 10

void main(void)
{
    vec3 face_normal = abs(inNormal[0] + inNormal[1] + inNormal[2]);

    uint maxi = face_normal[1] > face_normal[0] ? 1 : 0;
    maxi = face_normal[2] > face_normal[maxi] ? 2 : maxi;

    for (uint i = 0; i < 3; i++)
    {
        // voxel space pos
        outVoxelPos[i].pos = (inWorldPos[i] - VOXEL_DATA_CENTER) / VOXEL_DATA_SIZE;

        // project onto dominant axis
        if (maxi == 0)
        {
            outVoxelPos[i].xyz = outVoxelPos[i].zyx;
        }
        else if (maxi == 1)
        {
            outVoxelPos[i].xyz = outVoxelPos[i].xzy;
        }

        // projected pos
        outVoxelPos[i].xy /= VOXEL_DATA_RES;

        outVoxelPos[i].pos.z = 1;
        outNormal[i] = inNormal[i];
        outUV0[i] = inUV0[i];
        outUV1[i] = inUV1[i];
    }
}
