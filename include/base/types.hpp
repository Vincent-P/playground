#pragma once
#include "base/numerics.hpp"
#include "base/vectors.hpp"
#include <cassert>
#include <cstddef>

#define ARRAY_SIZE(_arr) (sizeof(_arr) / sizeof(*_arr))

#define MEMBER_OFFSET(type, member) (static_cast<u32>(reinterpret_cast<u64>(&reinterpret_cast<type *>(0)->member)))

#define not_implemented()                                                                                              \
    {                                                                                                                  \
        assert(false);                                                                                                 \
    }

#define PACKED __attribute__((packed))
#define UNUSED(x) (void)(x)


#if defined(__clang__)
#define CXX_CLANG
#elif defined(__GNUC__) || defined(__GNUG__)
#define CXX_GCC
#elif defined(_MSC_VER)
#define CXX_MSVC
#else
#define CXX_UNKNOWN
#endif

/// --- Constants

constexpr float PI = 3.1415926535897932384626433832795f;

constexpr float to_radians(float degres)
{
    // 180    -> PI
    // degres -> ?
    return degres * PI / 180.0f;
}

constexpr double to_radians(double degres)
{
    // 180    -> PI
    // degres -> ?
    return degres * PI / 180.0;
}

/// --- Vector types

struct int2
{
    i32 x;
    i32 y;

    bool operator==(const int2 &b) const = default;
};

struct uint2
{
    u32 x;
    u32 y;

    bool operator==(const uint2 &b) const = default;
};

struct uint3
{
    u32 x;
    u32 y;
    u32 z;

    bool operator==(const uint3 &b) const = default;
};

inline int2 operator+(const int2 &a, const int2 &b) { return {a.x + b.x, a.y + b.y}; }
inline int2 operator-(const int2 &a, const int2 &b) { return {a.x - b.x, a.y - b.y}; }
inline uint2 operator+(const uint2 &a, const uint2 &b) { return {a.x + b.x, a.y + b.y}; }
inline uint2 operator-(const uint2 &a, const uint2 &b) { return {a.x - b.x, a.y - b.y}; }
inline uint3 operator+(const uint3 &a, const uint3 &b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline uint3 operator-(const uint3 &a, const uint3 &b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

// --- User-defined literals

constexpr inline uint operator"" _K(unsigned long long value) { return value * 1000u; }

constexpr inline uint operator"" _KiB(unsigned long long value) { return value << 10; }

constexpr inline uint operator"" _MiB(unsigned long long value) { return value << 20; }

constexpr inline uint operator"" _GiB(unsigned long long value) { return value << 30; }
