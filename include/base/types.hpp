#pragma once
#include "base/numerics.hpp"
#include "base/vectors.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>

#define ARRAY_SIZE(_arr) (sizeof(_arr) / sizeof(*_arr))

#define MEMBER_OFFSET(type, member) (static_cast<u32>(reinterpret_cast<u64>(&reinterpret_cast<type *>(0)->member)))

#define not_implemented()                                                                                              \
    {                                                                                                                  \
        assert(false);                                                                                                 \
    }

#define PACKED

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
};

struct uint2
{
    u32 x;
    u32 y;
};

struct uint3
{
    u32 x;
    u32 y;
    u32 z;
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

/// --- Utility functions

template <typename T> inline T *ptr_offset(T *ptr, usize offset)
{
    return reinterpret_cast<T *>(reinterpret_cast<char *>(ptr) + offset);
}

template <typename E> inline constexpr auto to_underlying(E e) noexcept
{
    return static_cast<std::underlying_type_t<E>>(e);
}

template <typename vector_source, typename vector_dest, typename transform_function>
inline void map_transform(const vector_source &src, vector_dest &dst, transform_function f)
{
    dst.reserve(src.size());
    std::transform(src.begin(), src.end(), std::back_inserter(dst), f);
}

inline usize round_up_to_alignment(usize alignment, usize bytes)
{
    const usize mask = alignment - 1;
    return (bytes + mask) & ~mask;
}
