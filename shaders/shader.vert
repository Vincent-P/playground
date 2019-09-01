#version 450

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec3 inNormal;

layout(set = 0, binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
    mat4 clip;
    vec4 cam_pos;
    vec4 light_dir;
    float debugViewInput;
    float debugViewEquation;
    float ambient;
    float dummy;
} ubo;

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec3 outNormal;

#define VOXEL_DATA_RES 128

// flattened array index to 3D array index
uvec3 unflatten3D(uint idx, uvec3 dim)
{
	const uint z = idx / (dim.x * dim.y);
	idx -= (z * dim.x * dim.y);
	const uint y = idx / dim.x;
	const uint x = idx % dim.x;
	return  uvec3(x, y, z);
}

void main() {
    vec3 position = unflatten3D(gl_VertexIndex, uvec3(VOXEL_DATA_RES));
    outColor = inColor;
    outNormal = inNormal;
    gl_Position = vec4(position, 1.0);
}
