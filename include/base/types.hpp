#pragma once
#include <algorithm>
#include <cstddef>
#include <cassert>
#include "base/numerics.hpp"
#include "base/vectors.hpp"

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
    int x;
    int y;
};

struct uint2
{
    uint x;
    uint y;
};

// --- User-defined literals

inline uint operator"" _K(unsigned long long value)
{
    return value * 1000u;
}

inline uint operator"" _KiB(unsigned long long value)
{
    return value * 1024u;
}

inline uint operator"" _MiB(unsigned long long value)
{
    return value * 1024u * 1024u;
}


inline uint operator"" _GiB(unsigned long long value)
{
    return value * 1024u * 1024u * 1024;
}

/// --- Utility functions

template <typename T> inline T *ptr_offset(T *ptr, usize offset)
{
    return reinterpret_cast<T *>(reinterpret_cast<char *>(ptr) + offset);
}

template <typename E>
inline constexpr auto to_underlying(E e) noexcept
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
