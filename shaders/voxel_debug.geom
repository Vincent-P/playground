#version 450

layout (points) in;
layout (triangle_strip, max_vertices=36) out;

layout(set = 0, binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
    mat4 clip;
    vec4 cam_pos;
    vec4 light_dir;
    float debugViewInput;
    float debugViewEquation;
    float ambient;
    float cube_scale;
} ubo;

layout (location = 0) in vec3 inColor[];
layout (location = 1) in vec3 inNormal[];

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec3 outOffset;

#define MAKE_VERTEX(offset) \
        vertex = centerPos + ubo.cube_scale * vec4(offset, 0.0); \
        gl_Position =  ubo.clip * ubo.proj * ubo.view * vertex; \
        outOffset = offset; \
        outColor = inColor[0]; \
        outNormal = inNormal[0]; \
        EmitVertex();

// point -> cube
void main()
{
    vec4 centerPos = gl_in[0].gl_Position;
    vec4 vertex;

    if (inColor[0].x == 0 && inColor[0].y == 0 && inColor[0].z == 0)
        return;

    // -X
    {
        MAKE_VERTEX(vec3(-.5, .5, .5));
        MAKE_VERTEX(vec3(-.5, -.5, .5));
        MAKE_VERTEX(vec3(-.5, .5, -.5));
        EndPrimitive();
        MAKE_VERTEX(vec3(-.5, .5, -.5));
        MAKE_VERTEX(vec3(-.5, -.5, .5));
        MAKE_VERTEX(vec3(-.5, -.5, -.5));
        EndPrimitive();
    }

    // +X
    {
        MAKE_VERTEX(vec3(.5, .5, -.5));
        MAKE_VERTEX(vec3(.5, -.5, -.5));
        MAKE_VERTEX(vec3(.5, .5, .5));
        EndPrimitive();
        MAKE_VERTEX(vec3(.5, .5, .5));
        MAKE_VERTEX(vec3(.5, -.5, -.5));
        MAKE_VERTEX(vec3(.5, -.5, .5));
        EndPrimitive();
    }

    // -Y
    {
        MAKE_VERTEX(vec3(-.5, -.5, -.5));
        MAKE_VERTEX(vec3(-.5, -.5, .5));
        MAKE_VERTEX(vec3(.5, -.5, -.5));
        EndPrimitive();
        MAKE_VERTEX(vec3(.5, -.5, -.5));
        MAKE_VERTEX(vec3(-.5, -.5, .5));
        MAKE_VERTEX(vec3(.5, -.5, .5));
        EndPrimitive();
    }

    // +Y
    {
        MAKE_VERTEX(vec3(.5, .5, -.5));
        MAKE_VERTEX(vec3(.5, .5, .5));
        MAKE_VERTEX(vec3(-.5, .5, -.5));
        EndPrimitive();
        MAKE_VERTEX(vec3(-.5, .5, -.5));
        MAKE_VERTEX(vec3(.5, .5, .5));
        MAKE_VERTEX(vec3(-.5, .5, .5));
        EndPrimitive();
    }

    // -Z
    {
        MAKE_VERTEX(vec3(-.5, -.5, -.5));
        MAKE_VERTEX(vec3(.5, -.5, -.5));
        MAKE_VERTEX(vec3(-.5, .5, -.5));
        EndPrimitive();
        MAKE_VERTEX(vec3(-.5, .5, -.5));
        MAKE_VERTEX(vec3(.5, -.5, -.5));
        MAKE_VERTEX(vec3(.5, .5, -.5));
        EndPrimitive();
    }

    // +Z
    {
        MAKE_VERTEX(vec3(.5, -.5, .5));
        MAKE_VERTEX(vec3(-.5, -.5, .5));
        MAKE_VERTEX(vec3(.5, .5, .5));
        EndPrimitive();
        MAKE_VERTEX(vec3(.5, .5, .5));
        MAKE_VERTEX(vec3(-.5, -.5, .5));
        MAKE_VERTEX(vec3(-.5, .5, .5));
        EndPrimitive();
    }
}
