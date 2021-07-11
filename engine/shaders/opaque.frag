#version 460

// http://www.jcgt.org/published/0009/03/02/
uvec3 pcg3d(uvec3 v) {

    v = v * 1664525u + 1013904223u;

    v.x += v.y*v.z;
    v.y += v.z*v.x;
    v.z += v.x*v.y;

    v ^= v >> 16u;

    v.x += v.y*v.z;
    v.y += v.z*v.x;
    v.z += v.x*v.y;

    return v;
}

uvec3 hash(vec2 s)
{
    uvec4 u = uvec4(s, uint(s.x) ^ uint(s.y), uint(s.x) + uint(s.y));
    return pcg3d(u.xyz);
}

layout(set = 0, binding = 0, std140) uniform GlobalUniform
{
    mat4 camera_view;
    mat4 camera_projection;
    mat4 camera_view_inverse;
    mat4 camera_projection_inverse;
    mat4 camera_previous_view;
    mat4 camera_previous_projection;
    vec4 camera_position;
    uint mdr0;
    uint mdr2;
    uint mdr3;
    uint mdr4;
    vec2 resolution;
    float delta_t;
    uint frame_count;
    uint camera_moved;
    uint render_texture_offset;
    vec2 jitter_offset;
    uint is_path_tracing;
} globals;

layout(push_constant, std430) uniform PushConstants
{
    uint draw_idx;
    uint render_mesh_idx;
} push_constants;

layout(set = 1, binding = 0) uniform sampler2D global_textures[1];
layout(set = 1, binding = 0) uniform sampler3D global_textures_3d[1];
layout(set = 2, binding = 0, rgba8) uniform readonly writeonly image2D global_images_2d_rgba8[1];
layout(set = 2, binding = 0, rgba32f) uniform readonly writeonly image2D global_images_2d_rgba32f[1];
layout(set = 2, binding = 0, r32f) uniform readonly writeonly image2D global_images_2d_r32f[1];

layout(location = 0) out vec4 o_color;
layout(location = 0) flat in uint i_triangle_id;

void main()
{
        vec2 seed = vec2(gl_PrimitiveID % 256, gl_PrimitiveID / 256);
	o_color = vec4(vec3(hash(seed)) * (1.0/float(0xffffffffu)), 1.0);
}
