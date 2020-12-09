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

struct Box {
    float3      center;
    float3     radius;
    float3     invRadius;
};

struct Ray {
    float3      origin;
    /** Unit direction of propagation */
    float3     direction;
};

#endif
