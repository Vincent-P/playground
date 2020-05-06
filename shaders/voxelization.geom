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

layout(set = 1, binding = 1) uniform UBO {
    vec3 center;
    float size;
    uint res;
} debug_options;

void main(void)
{
    vec3 face_normal = abs(inNormal[0] + inNormal[1] + inNormal[2]);

    uint maxi = face_normal[1] > face_normal[0] ? 1 : 0;
    maxi = face_normal[2] > face_normal[maxi] ? 2 : maxi;

    for (uint i = 0; i < gl_in.length(); i++)
    {
        // voxel space pos
        gl_Position = vec4((inWorldPos[i] - debug_options.center) / debug_options.size, 1.0);

        // project onto dominant axis
        if (maxi == 0)
        {
            gl_Position.xyz = gl_Position.zyx;
        }
        else if (maxi == 1)
        {
            gl_Position.xyz = gl_Position.xzy;
        }

        // projected pos
        gl_Position.xy /= debug_options.res;
        gl_Position.z = 1;

        outWorldPos = inWorldPos[i];
        outNormal = inNormal[i];
        outUV0 = inUV0[i];
        outUV1 = inUV1[i];
        EmitVertex();
    }
    EndPrimitive();
}
