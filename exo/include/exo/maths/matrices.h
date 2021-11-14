#pragma once
#include "exo/maths/vectors.h"

namespace exo
{
struct float4x4;

float4x4 transpose(const float4x4 &m);

struct float4x4
{
    // store values in column-major to match glsl
    float values[16];


    float4x4(float value = 0.0f);
    // take values in row-major order!!!
    float4x4(const float (&_values)[16]);

    static float4x4 identity();

    const float &at(usize row, usize col) const;
    float &at(usize row, usize col);

    // Returns a column of a matrix, starts at 0
    const float4 &col(usize col) const;
    float4 &col(usize col);

    bool operator==(const float4x4 &b) const = default;
};

float4x4 operator+(const float4x4 &a, const float4x4 &b);
float4x4 operator-(const float4x4 &a, const float4x4 &b);
float4x4 operator*(float a, const float4x4 &m);

float4x4 operator*(const float4x4 &a, const float4x4 &b);
float4   operator*(const float4x4 &m, const float4 &v);

float4x4 inverse_transform(const float4x4 &transform);
}
