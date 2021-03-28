#ifndef MATHS_H
#define MATHS_H

float max2(vec2 values) {
    return max(values.x, values.y);
}

float max3(vec3 values) {
    return max(values.x, max(values.y, values.z));
}

float max4(vec4 values) {
    return max(max(values.x, values.y), max(values.z, values.w));
}

float min2(vec2 values) {
    return min(values.x, values.y);
}

float min3(vec3 values) {
    return min(values.x, min(values.y, values.z));
}

float min4(vec4 values) {
    return min(min(values.x, values.y), min(values.z, values.w));
}

uint init_seed(uint2 pixel_pos, uint frame_count)
{
    return uint(uint(pixel_pos.x) * uint(1973) + uint(pixel_pos.y) * uint(9277) + uint(frame_count) * uint(26699)) | uint(1);
}

uint wang_hash(inout uint seed)
{
    seed = uint(seed ^ uint(61)) ^ uint(seed >> uint(16));
    seed *= uint(9);
    seed = seed ^ (seed >> 4);
    seed *= uint(0x27d4eb2d);
    seed = seed ^ (seed >> 15);
    return seed;
}

float random_float_01(inout uint seed)
{
    return float(wang_hash(seed)) / 4294967296.0;
}
#endif
