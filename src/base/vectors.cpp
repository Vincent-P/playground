#include "base/types.hpp"

#include <cmath>
#include <cstring>
#include <doctest.h>
#include <iostream>


float dot(const float2 &a, const float2 &b)
{
    return a.x*b.x + a.y*b.y;
}

float dot(const float3 &a, const float3 &b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

float dot(const float4 &a, const float4 &b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
}

template <typename T>
static T normalize_impl(const T &v)
{
    T normalized = v;
    float inv_sqrt = 1.0f / v.norm();
    normalized = inv_sqrt * normalized;
    return normalized;
}

// clang-format off
float2 normalize(const float2 &v) { return normalize_impl(v); }
float3 normalize(const float3 &v) { return normalize_impl(v); }
float4 normalize(const float4 &v) { return normalize_impl(v); }
// clang-format on

float2 round(const float2 &v)
{
    return float2(
        std::round(v.x),
        std::round(v.y)
    );
}

float3 round(const float3 &v)
{
    return float3(
        std::round(v.x),
        std::round(v.y),
        std::round(v.z)
    );
}

float4 round(const float4 &v)
{
    return float4(
        std::round(v.x),
        std::round(v.y),
        std::round(v.z),
        std::round(v.w)
    );
}

float3 cross(const float3 &a, const float3 &b)
{
    return {
        a.y * b.z - b.y * a.z,
        a.z * b.x - b.z * a.x,
        a.x * b.y - b.x * a.y
    };
}
// clang-format off
float float2::squared_norm() const { return dot(*this, *this); }
float float3::squared_norm() const { return dot(*this, *this); }
float float4::squared_norm() const { return dot(*this, *this); }
float float2::norm() const { return std::sqrt(squared_norm()); }
float float3::norm() const { return std::sqrt(squared_norm()); }
float float4::norm() const { return std::sqrt(squared_norm()); }

bool operator==(const float2 &a, const float2 &b)  { return a.x == b.x && a.y == b.y; }
float2 operator+(const float2 &a, const float2 &b) { return {a.x + b.x, a.y + b.y}; }
float2 operator-(const float2 &a, const float2 &b) { return {a.x - b.x, a.y - b.y}; }
float2 operator*(const float2 &a, const float2 &b) { return {a.x * b.x, a.y * b.y}; }
float2 operator*(const float a, const float2 &v)   { return {a * v.x, a * v.y}; }

bool operator==(const float3 &a, const float3 &b)  { return a.x == b.x && a.y == b.y && a.z == b.z; }
float3 operator+(const float3 &a, const float3 &b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
float3 operator-(const float3 &a, const float3 &b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
float3 operator*(const float3 &a, const float3 &b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
float3 operator*(const float a, const float3 &v)   { return {a * v.x, a * v.y, a * v.z}; }

bool operator==(const float4 &a, const float4 &b)   { return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; }
float4 operator+(const float4 &a, const float4 &b)  { return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }
float4 operator-(const float4 &a, const float4 &b)  { return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w}; }
float4 operator*(const float4 &a, const float4 &b)  { return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w}; }
float4 operator*(const float a, const float4 &v)    { return {a * v.x, a * v.y, a * v.z, a * v.w}; }
// clang-format on

std::ostream& operator<<(std::ostream& os, const float2 &v)
{
    os << "(" << v.x << ", " << v.y << ")";
    return os;
}
std::ostream& operator<<(std::ostream& os, const float3 &v)
{
    os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    return os;
}
std::ostream& operator<<(std::ostream& os, const float4 &v)
{
    os << "(" << v.x << ", " << v.y <<  ", " << v.z << ", " << v.w << ")";
    return os;
}

float4x4::float4x4(float value)
{
    for (usize i = 0; i < ARRAY_SIZE(values); i++)
    {
        values[i] = value;
    }
}

float4x4::float4x4(const float (&_values)[16])
{
    for (uint col = 0; col < 4; col++)
        for (uint row = 0; row < 4; row++)
            this->at(row, col) = _values[row * 4 + col];
}

float4x4 float4x4::identity()
{
    return float4x4({
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        });
}

float float4x4::at(usize row, usize col) const
{
    // values are stored in columns
    return values[col * 4 + row];
}

float &float4x4::at(usize row, usize col)
{
    // values are stored in columns
    return values[col * 4 + row];
}

float4x4 transpose(const float4x4 &m)
{
    float4x4 result;
    for (usize row = 0; row < 4; row++)
        for (usize col = 0; col < 4; col++)
            result.at(row, col) = m.at(col, row);
    return result;
}

float4x4 operator*(float a, const float4x4 &m)
{
    float4x4 result;
    for (usize i = 0; i < 16; i++)
        result.values[i] = a * m.values[i];
    return result;
}

float4 operator*(const float4x4 &m, const float4 &v)
{
    float4 result;
    for (usize row = 0; row < 4; row++)
        for (usize col = 0; col < 4; col++)
            result.raw[row] += m.at(row, col) * v.raw[col];

    return result;
}

bool operator==(const float4x4 &a, const float4x4 &b)
{
    return std::memcmp(a.values, b.values, sizeof(a.values)) == 0;
}

float4x4 operator+(const float4x4 &a, const float4x4 &b)
{
    float4x4 result;
    for (usize col = 0; col < 4; col++)
        for (usize row = 0; row < 4; row++)
            result.at(row, col) = a.at(row, col) + b.at(row, col);
    return result;
}

float4x4 operator-(const float4x4 &a, const float4x4 &b)
{
    float4x4 result;
    for (usize col = 0; col < 4; col++)
        for (usize row = 0; row < 4; row++)
            result.at(row, col) = a.at(row, col) - b.at(row, col);
    return result;
}

float4x4 operator*(const float4x4 &a, const float4x4 &b)
{
    float4x4 result;
    for (usize col = 0; col < 4; col++)
        for (usize row = 0; row < 4; row++)
            for (usize i = 0; i < 4; i++)
                result.at(row, col) += a.at(row, i) * b.at(i, col);
    return result;
}

/// --- Tests

TEST_SUITE("Vectors")
{
    TEST_CASE("operators")
    {
        CHECK(float2(1.0f, 2.0f) + float2(3.0f, 4.0f) == float2(4.0f, 6.0f));
        CHECK(float2(1.0f, 2.0f) - float2(3.0f, 4.0f) == float2(-2.0f));
        CHECK(float2(1.0f, 2.0f) * float2(3.0f, 4.0f) == float2(3.0f, 8.0f));

        CHECK(float3(1.0f, 2.0f, 3.0f) + float3(4.0f, 5.0f, 6.0f) == float3(5.0f, 7.0f, 9.0f));
        CHECK(float3(1.0f, 2.0f, 3.0f) - float3(4.0f, 5.0f, 6.0f) == float3(-3.0f));
        CHECK(float3(1.0f, 2.0f, 3.0f) * float3(4.0f, 5.0f, 6.0f) == float3(4.0f, 10.0f, 18.0f));

        CHECK(float4(1.0f, 2.0f, 3.0f, 4.0f) + float4(5.0f, 6.0f, 7.0f, 8.0f) == float4(6.0f, 8.0f, 10.0f, 12.0f));
        CHECK(float4(1.0f, 2.0f, 3.0f, 4.0f) - float4(5.0f, 6.0f, 7.0f, 8.0f) == float4(-4.0f));
        CHECK(float4(1.0f, 2.0f, 3.0f, 4.0f) * float4(5.0f, 6.0f, 7.0f, 8.0f) == float4(5.0f, 12.0f, 21.0f, 32.0f));
    }

    TEST_CASE("members functions")
    {
        CHECK(float2(1.0f, 2.0f).squared_norm() == 5.0f);
        CHECK(float2(1.0f, 2.0f).norm() == sqrt(5.0f));

        CHECK(float3(1.0f, 2.0f, 3.0f).squared_norm() == 14.0f);
        CHECK(float3(1.0f, 2.0f, 3.0f).norm() == sqrt(14.0f));

        CHECK(float4(1.0f, 2.0f, 3.0f, 4.0f).squared_norm() == 30.0f);
        CHECK(float4(1.0f, 2.0f, 3.0f, 4.0f).norm() == sqrt(30.0f));
    }

    TEST_CASE("maths")
    {
        auto v2 = float2(1.0f, 2.0f);
        auto v3 = float3(1.0f, 2.0f, 3.0f);
        auto v4 = float4(1.0f, 2.0f, 3.0f, 4.0f);

        CHECK(dot(v2, float2(1.0f, 0.0f)) == 1.0f);
        CHECK(dot(v2, float2(0.0f, 1.0f)) == 2.0f);
        CHECK(normalize(v2).norm() == doctest::Approx(normalize(v2).squared_norm())); // approx compares with a small epsilon
        CHECK(round(1.5f * v2) == float2(2.0f, 3.0f));

        CHECK(dot(v3, float3(1.0f, 0.0f, 0.0f)) == 1.0f);
        CHECK(dot(v3, float3(0.0f, 1.0f, 0.0f)) == 2.0f);
        CHECK(dot(v3, float3(0.0f, 0.0f, 1.0f)) == 3.0f);
        CHECK(normalize(v3).norm() == doctest::Approx(normalize(v3).squared_norm())); // approx compares with a small epsilon
        CHECK(round(1.5f * v3) == float3(2.0f, 3.0f, 5.0f));

        CHECK(dot(v4, float4(1.0f, 0.0f, 0.0f, 0.0f)) == 1.0f);
        CHECK(dot(v4, float4(0.0f, 1.0f, 0.0f, 0.0f)) == 2.0f);
        CHECK(dot(v4, float4(0.0f, 0.0f, 1.0f, 0.0f)) == 3.0f);
        CHECK(dot(v4, float4(0.0f, 0.0f, 0.0f, 1.0f)) == 4.0f);
        CHECK(normalize(v4).norm() == doctest::Approx(normalize(v4).squared_norm())); // approx compares with a small epsilon
        CHECK(round(1.5f * v4) == float4(2.0f, 3.0f, 5.0f, 6.0f));
    }

    TEST_CASE("cross product")
    {
        float3 x{1.0f, 0.0f, 0.0f};
        float3 y{0.0f, 1.0f, 0.0f};
        float3 z{0.0f, 0.0f, 1.0f};

        CHECK(cross(x, y) == z);
        CHECK(cross(y, z) == x);
        CHECK(cross(z, x) == y);

        CHECK(cross(y, x) == -1.0f * z);
        CHECK(cross(z, y) == -1.0f * x);
        CHECK(cross(x, z) == -1.0f * y);

    }
}

TEST_SUITE("Matrices")
{
    TEST_CASE("operators")
    {
        float4x4 identity = float4x4::identity();
        float4x4 expected{{
                2.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 2.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 2.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 2.0f,
            }};
        CHECK(2.0f * identity == expected);
    }

    TEST_CASE("Matrix multiplication")
    {
        float4x4 identity({
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f,
            });

        float4x4 m({
                1.0f, 2.0f, 0.0f, 0.0f,
                2.0f, 1.0f, 4.0f, 0.0f,
                0.0f, 9.0f, 1.0f, 0.0f,
                3.0f, 0.0f, 8.0f, 1.0f,
            });

        CHECK(identity * (m * identity) == m);

        float4 v{ 1.0f, 2.0f, 3.0f, 4.0f };

        float4 expected{5.0f, 16.0f, 21.0f, 31.0f};
        CHECK(m * v == expected);
    }
}
