#pragma once
#include "exo/numerics.h"

struct uint2;
struct uint3;
struct int2;
struct int3;
struct float2;
struct float3;
struct float4;

// -- Functions
float  max(float2 v);
float  max(float3 v);
float  max(float4 v);
usize  max_comp(float2 v);
usize  max_comp(float3 v);
usize  max_comp(float4 v);
float  length(float2 v);
float  length(float3 v);
float  length(float4 v);
float  distance(float2 a, float2 b);
float  distance(float3 a, float3 b);
float  distance(float4 a, float4 b);
float  dot(float2 a, float2 b);
float  dot(float3 a, float3 b);
float  dot(float4 a, float4 b);
float2 normalize(float2 v);
float3 normalize(float3 v);
float4 normalize(float4 v);
float2 round(float2 v);
float3 round(float3 v);
float4 round(float4 v);
float2 floor(float2 v);
float3 floor(float3 v);
float4 floor(float4 v);
float3 cross(float3 a, float3 b);

// -- Structs (no member functions except for swizzles, and raw access)

// Access components using the [] operator, also provides pointers to components
#define VECTOR_RAW_ACCESS(vector_type, size, inner_type)                                                                                                                                                                   \
    static constexpr usize SIZE = size;                                                                                                                                                                                    \
    using element_type          = inner_type;                                                                                                                                                                              \
    element_type &operator[](usize i_component)                                                                                                                                                                            \
    {                                                                                                                                                                                                                      \
        return data()[i_component];                                                                                                                                                                                        \
    }                                                                                                                                                                                                                      \
    const element_type &operator[](usize i_component) const                                                                                                                                                                \
    {                                                                                                                                                                                                                      \
        return data()[i_component];                                                                                                                                                                                        \
    }                                                                                                                                                                                                                      \
    constexpr element_type *data()                                                                                                                                                                                         \
    {                                                                                                                                                                                                                      \
        return &x;                                                                                                                                                                                                         \
    }                                                                                                                                                                                                                      \
    constexpr const element_type *data() const                                                                                                                                                                             \
    {                                                                                                                                                                                                                      \
        return &x;                                                                                                                                                                                                         \
    }

struct int2
{
    i32 x;
    i32 y;

    constexpr int2(i32 _x, i32 _y);
    constexpr int2(i32 val);

    // conversions
    explicit constexpr int2(float2 v);
    explicit constexpr int2(uint2 v);

    bool operator==(const int2 &b) const = default;
    VECTOR_RAW_ACCESS(int2, 2, i32)

    // Access swizzle for 2D vectors
    // clang-format off
    #include "vectors_swizzle.h"
    #define VEC2_S2(a, b) constexpr int2 a##b() const { return {a, b}; }
    VEC2_SWIZZLES
    #undef VEC2_S2
    // clang-format on
};

struct uint2
{
    u32 x;
    u32 y;

    constexpr uint2(u32 _x, u32 _y);
    constexpr uint2(u32 val = 0.0f);

    // conversions
    explicit constexpr uint2(float2 v);
    explicit constexpr uint2(int2 v);

    bool operator==(const uint2 &b) const = default;
    VECTOR_RAW_ACCESS(uint2, 2, u32)

    // Access swizzle for 2D vectors
    // clang-format off
    #include "vectors_swizzle.h"
    #define VEC2_S2(a, b) constexpr uint2 a##b() const { return {a, b}; }
    VEC2_SWIZZLES
    #undef VEC2_S2
    // clang-format on
};

struct float2
{
    float x;
    float y;

    constexpr float2(float _x, float _y);
    constexpr float2(float val = 0.0f);

    // conversions
    explicit constexpr float2(uint2 v);
    explicit constexpr float2(int2 v);

    bool operator==(const float2 &b) const = default;
    VECTOR_RAW_ACCESS(float2, 2, float)

    // Access swizzle for 2D vectors
    // clang-format off
    #include "vectors_swizzle.h"
    #define VEC2_S2(a, b) constexpr float2 a##b() const { return {a, b}; }
    VEC2_SWIZZLES
    #undef VEC2_S2
    // clang-format on
};

struct int3
{
    i32 x;
    i32 y;
    i32 z;

    constexpr int3(i32 _x, i32 _y, i32 _z);
    constexpr int3(int2 v, i32 z);
    constexpr int3(i32 val = 0);

    // conversions
    explicit constexpr int3(uint3 v);
    explicit constexpr int3(float3 v);

    bool operator==(const int3 &b) const = default;
    VECTOR_RAW_ACCESS(int3, 3, i32)

// Access swizzle for 3D vectors
// clang-format off
    #include "vectors_swizzle.h"
    #define VEC3_S3(a, b, c) constexpr int3 a##b##c() const { return {a, b, c}; }
    VEC3_SWIZZLES
    #undef VEC3_S2
    #define VEC3_S2(a, b) constexpr int2 a##b() const { return {a, b}; }
    VEC3_SWIZZLES
    #undef VEC3_S3
    #undef VEC3_S2
    // clang-format on
};

struct uint3
{
    u32 x;
    u32 y;
    u32 z;

    constexpr uint3(u32 _x, u32 _y, u32 _z);
    constexpr uint3(uint2 v, u32 z);
    constexpr uint3(u32 val = 0u);

    // conversions
    explicit constexpr uint3(int3 v);
    explicit constexpr uint3(float3 v);

    bool operator==(const uint3 &b) const = default;
    VECTOR_RAW_ACCESS(uint3, 3, u32)

    // Access swizzle for 3D vectors
    // clang-format off
    #include "vectors_swizzle.h"
    #define VEC3_S3(a, b, c) constexpr uint3 a##b##c() const { return {a, b, c}; }
    VEC3_SWIZZLES
    #undef VEC3_S2
    #define VEC3_S2(a, b) constexpr uint2 a##b() const { return {a, b}; }
    VEC3_SWIZZLES
    #undef VEC3_S2
    #undef VEC3_S3
    // clang-format on
};

struct float3
{
    float x;
    float y;
    float z;

    constexpr float3(float _x, float _y, float _z);
    constexpr float3(float2 v2, float z);
    constexpr float3(float _x);

    // conversions
    explicit constexpr float3(uint3 v);
    explicit constexpr float3(int3 v);

    bool operator==(const float3 &b) const = default;
    VECTOR_RAW_ACCESS(float3, 3, float)

    // Access swizzle for 3D vectors
    // clang-format off
    #include "vectors_swizzle.h"
    #define VEC3_S3(a, b, c) constexpr float3 a##b##c() const { return {a, b, c}; }
    VEC3_SWIZZLES
    #undef VEC3_S2
    #define VEC3_S2(a, b) constexpr float2 a##b() const { return {a, b}; }
    VEC3_SWIZZLES
    #undef VEC3_S2
    #undef VEC3_S3
    // clang-format on
};

struct float4
{
    float x;
    float y;
    float z;
    float w;

    constexpr float4(float _x, float _y, float _z, float _w);
    constexpr float4(float2 v, float _z, float _w);
    constexpr float4(float3 v, float _w);
    constexpr float4(float val = 0.0f);

    bool operator==(const float4 &b) const = default;
    VECTOR_RAW_ACCESS(float4, 4, float)

    // Access swizzle for 4D vectors
    // clang-format off
    #include "vectors_swizzle.h"
    #define VEC4_S4(a, b, c, d) constexpr float4 a##b##c##d() const { return {a, b, c, d}; }
    VEC4_SWIZZLES
    #undef VEC4_S3
    #define VEC4_S3(a, b, c) constexpr float3 a##b##c() const { return {a, b, c}; }
    VEC4_SWIZZLES
    #undef VEC4_S2
    #define VEC4_S2(a, b) constexpr float2 a##b() const { return {a, b}; }
    VEC4_SWIZZLES
    #undef VEC4_S2
    #undef VEC4_S3
    #undef VEC4_S4
    // clang-format on
};

#undef VEC2_SWIZZLES
#undef VEC3_SWIZZLES
#undef VEC4_SWIZZLES

// -- Constructors
// clang-format off
constexpr int2::int2(i32 _x, i32 _y) : x{_x}, y{_y} {}
constexpr int2::int2(i32 val) : int2{val, val} {}
constexpr int2::int2(float2 v) : int2{static_cast<i32>(v.x), static_cast<i32>(v.y)} {}
constexpr int2::int2(uint2 v) : int2{static_cast<i32>(v.x), static_cast<i32>(v.y)} {}

constexpr uint2::uint2(u32 _x, u32 _y) : x{_x}, y{_y} {}
constexpr uint2::uint2(u32 val) : uint2{val, val} {}
constexpr uint2::uint2(float2 v) : uint2{static_cast<u32>(v.x), static_cast<u32>(v.y)} {}
constexpr uint2::uint2(int2 v) : uint2{static_cast<u32>(v.x), static_cast<u32>(v.y)} {}

constexpr float2::float2(float _x, float _y) : x{_x}, y{_y} {}
constexpr float2::float2(float val) : float2{val, val} {}
constexpr float2::float2(uint2 v) : float2{static_cast<float>(v.x), static_cast<float>(v.y)} {}
constexpr float2::float2(int2 v) : float2{static_cast<float>(v.x), static_cast<float>(v.y)} {}

constexpr int3::int3(i32 _x, i32 _y, i32 _z) : x{_x}, y{_y}, z{_z} {}
constexpr int3::int3(int2 v, i32 z) : int3{v.x, v.y, z} {}
constexpr int3::int3(i32 val) : int3{val, val, val} {}
constexpr int3::int3(float3 v) : int3{static_cast<i32>(v.x), static_cast<i32>(v.y), static_cast<i32>(v.z)} {}
constexpr int3::int3(uint3 v) : int3{static_cast<i32>(v.x), static_cast<i32>(v.y), static_cast<i32>(v.z)} {}

constexpr uint3::uint3(u32 _x, u32 _y, u32 _z) : x{_x}, y{_y}, z{_z} {}
constexpr uint3::uint3(u32 val) : uint3{val, val, val} {}
constexpr uint3::uint3(float3 v) : uint3{static_cast<u32>(v.x), static_cast<u32>(v.y), static_cast<u32>(v.z)} {}
constexpr uint3::uint3(int3 v) : uint3{static_cast<u32>(v.x), static_cast<u32>(v.y), static_cast<u32>(v.z)} {}

constexpr float3::float3(float _x, float _y, float _z) : x{_x}, y{_y}, z{_z} {}
constexpr float3::float3(float val) : float3{val, val, val} {}
constexpr float3::float3(uint3 v) : float3{static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)} {}
constexpr float3::float3(int3 v) : float3{static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)} {}

constexpr float4::float4(float _x, float _y, float _z, float _w) : x{_x}, y{_y}, z{_z}, w{_w} {}
constexpr float4::float4(float2 v, float _z, float _w) : float4{v.x, v.y, _z, _w} {}
constexpr float4::float4(float3 v, float _w) : float4{v.x, v.y, v.z, _w} {}
constexpr float4::float4(float val) : float4{val, val, val, val} {}
// clang-format on

// -- Inline operators
// clang-format off
constexpr int2 operator+(int2 a, int2 b)  { return {a.x + b.x, a.y + b.y}; }
constexpr int2 operator-(int2 a, int2 b)  { return {a.x - b.x, a.y - b.y}; }
constexpr int2 operator*(int2 a, int2 b)  { return {a.x * b.x, a.y * b.y}; }
constexpr int2 operator*(i32 a, int2 b)   { return {a * b.x, a * b.y}; }

constexpr uint2 operator+(uint2 a, uint2 b) { return {a.x + b.x, a.y + b.y}; }
constexpr uint2 operator-(uint2 a, uint2 b) { return {a.x - b.x, a.y - b.y}; }
constexpr uint2 operator*(uint2 a, uint2 b) { return {a.x * b.x, a.y * b.y}; }
constexpr uint2 operator*(u32 a, uint2 b)   { return {a * b.x, a * b.y}; }

constexpr float2 operator+(float2 a, float2 b) { return {a.x + b.x, a.y + b.y}; }
constexpr float2 operator-(float2 a, float2 b) { return {a.x - b.x, a.y - b.y}; }
constexpr float2 operator*(float2 a, float2 b) { return {a.x * b.x, a.y * b.y}; }
constexpr float2 operator*(float a, float2 b)  { return {a * b.x, a * b.y}; }

constexpr int3 operator+(int3 a, int3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
constexpr int3 operator-(int3 a, int3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
constexpr int3 operator*(int3 a, int3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
constexpr int3 operator*(i32 a, int3 b)  { return {a * b.x, a * b.y, a * b.z}; }

constexpr uint3 operator+(uint3 a, uint3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
constexpr uint3 operator-(uint3 a, uint3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
constexpr uint3 operator*(uint3 a, uint3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
constexpr uint3 operator*(u32 a, uint3 b)   { return {a * b.x, a * b.y, a * b.z}; }

constexpr float3 operator+(float3 a, float3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
constexpr float3 operator-(float3 a, float3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
constexpr float3 operator*(float3 a, float3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
constexpr float3 operator*(float a, float3 b)  { return {a * b.x, a * b.y, a * b.z}; }

constexpr float4 operator+(float4 a, float4 b) { return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }
constexpr float4 operator-(float4 a, float4 b) { return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w}; }
constexpr float4 operator*(float4 a, float4 b) { return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w}; }
constexpr float4 operator*(float a, float4 b)  { return {a * b.x, a * b.y, a * b.z, a * b.w}; }
// clang-format on

// -- Constants
inline constexpr auto float3_RIGHT   = float3{1, 0, 0};
inline constexpr auto float3_UP      = float3{0, 1, 0};
inline constexpr auto float3_FORWARD = float3{0, 0, -1};
