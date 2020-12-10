#pragma once
#include "base/numerics.hpp"
#include <iosfwd>

struct float3;
struct float4;

struct float2
{
    constexpr float2(float _x, float _y)
        : x(_x)
        , y(_y)
    {
    }

    constexpr float2(float _x = 0.0f)
        : float2(_x, _x)
    {
    }

    float squared_norm() const;
    float norm() const;
    inline float *data() { return raw; }

    union
    {
        struct
        {
            float x;
            float y;
        };
        float raw[2];
    };

    /// --- swizzle black magic
    // clang-format off
    #include "vectors_swizzle.h"
    #define VEC2_S2(a, b) float2 a##b() { return {a, b}; }
    VEC2_SWIZZLES
    // clang-format on

    friend std::ostream& operator<<(std::ostream& os, const float2 &v);
};

struct float3
{
    constexpr float3(float _x, float _y, float _z)
        : x(_x)
        , y(_y)
        , z(_z)
    {
    }

    constexpr float3(float _x = 0.0f)
        : float3(_x, _x, _x)
    {
    }

    float squared_norm() const;
    float norm() const;
    inline float *data() { return raw; }

    union
    {
        struct
        {
            float x;
            float y;
            float z;
        };
        float raw[3];
    };

    /// --- swizzle black magic
    // clang-format off
    #include "vectors_swizzle.h"
    #define VEC3_S3(a, b, c) float3 a##b##c() { return {a, b, c}; }
    VEC3_SWIZZLES
    #undef VEC3_S2
    #define VEC3_S2(a, b) float2 a##b() { return {a, b}; }
    VEC3_SWIZZLES
    // clang-format on

    friend std::ostream& operator<<(std::ostream& os, const float3 &v);
};

struct float4
{
    constexpr float4(float _x, float _y, float _z, float _w)
        : x(_x)
        , y(_y)
        , z(_z)
        , w(_w)
    {
    }

    constexpr float4(float _x = 0.0f)
        : float4(_x, _x, _x, _x)
    {
    }
    constexpr float4(float3 v3, float _w)
        : float4(v3.x, v3.y, v3.z, _w)
    {
    }

    float squared_norm() const;
    float norm() const;
    inline float *data() { return raw; }

    union
    {
        struct
        {
            float x;
            float y;
            float z;
            float w;
        };
        struct
        {
            float r;
            float g;
            float b;
            float a;
        };
        float raw[4];
    };

    /// --- swizzle black magic
    // clang-format off
    #include "vectors_swizzle.h"
    #define VEC4_S4(a, b, c, d) float4 a##b##c##d() { return {a, b, c, d}; }
    VEC4_SWIZZLES
    #undef VEC4_S3
    #define VEC4_S3(a, b, c) float3 a##b##c() { return {a, b, c}; }
    VEC4_SWIZZLES
    #undef VEC4_S2
    #define VEC4_S2(a, b) float2 a##b() { return {a, b}; }
    VEC4_SWIZZLES
    // clang-format on

    friend  std::ostream& operator<<(std::ostream& os, const float4 &v);
};

struct float4x4
{
    float4x4(float value = 0.0f);

    // take values in row-major order!!!
    float4x4(const float (&_values)[16]);

    static float4x4 identity();
    float at(usize row, usize col) const;
    float &at(usize row, usize col);

    // store values in column-major to match glsl
    float values[16];
};

static_assert(sizeof(float2) == 2 * sizeof(float));
static_assert(sizeof(float3) == 3 * sizeof(float));
static_assert(sizeof(float4) == 4 * sizeof(float));
static_assert(sizeof(float4x4) == 4 * sizeof(float4));

float dot(const float2 &a, const float2 &b);
float dot(const float3 &a, const float3 &b);
float dot(const float4 &a, const float4 &b);
float2 normalize(const float2 &v);
float3 normalize(const float3 &v);
float4 normalize(const float4 &v);
float2 round(const float2 &v);
float3 round(const float3 &v);
float4 round(const float4 &v);

float3 cross(const float3 &a, const float3 &b);

bool operator==(const float2 &a, const float2 &b);
float2 operator+(const float2 &a, const float2 &b);
float2 operator-(const float2 &a, const float2 &b);
float2 operator*(const float2 &a, const float2 &b);
float2 operator*(const float a, const float2 &v);

bool operator==(const float3 &a, const float3 &b);
float3 operator+(const float3 &a, const float3 &b);
float3 operator-(const float3 &a, const float3 &b);
float3 operator*(const float3 &a, const float3 &b);
float3 operator*(const float a, const float3 &v);

bool operator==(const float4 &a, const float4 &b);
float4 operator+(const float4 &a, const float4 &b);
float4 operator-(const float4 &a, const float4 &b);
float4 operator*(const float4 &a, const float4 &b);
float4 operator*(const float a, const float4 &v);

float4x4 transpose(const float4x4 &m);

bool operator==(const float4x4 &a, const float4x4 &b);
float4x4 operator+(const float4x4 &a, const float4x4 &b);
float4x4 operator-(const float4x4 &a, const float4x4 &b);
float4x4 operator*(float a, const float4x4 &m);
float4x4 operator*(const float4x4 &a, const float4x4 &b);
float4 operator*(const float4x4 &m, const float4 &v);

inline constexpr auto float3_RIGHT   = float3(1, 0, 0);
inline constexpr auto float3_UP      = float3(0, 1, 0);
inline constexpr auto float3_FORWARD = float3(0, 0, -1);
