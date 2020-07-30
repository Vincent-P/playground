layout (set = 0, binding = 0) uniform GlobalUniform {
    mat4 camera_view;
    mat4 camera_proj;
    mat4 camera_inv_proj;
    mat4 sun_view;
    mat4 sun_proj;
} global;
