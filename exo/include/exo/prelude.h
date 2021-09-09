#pragma once

#if defined(__clang__)
#define CXX_CLANG
#elif defined(__GNUC__) || defined(__GNUG__)
#define CXX_GCC
#elif defined(_MSC_VER)
#define CXX_MSVC
#else
#define CXX_UNKNOWN
#endif

#include "exo/maths/numerics.h"
#include "exo/maths/vectors.h"
#include "exo/maths/matrices.h"

#if !defined(NDEBUG)
#include <cassert>
#else
#define assert(x)
#endif
#include <cstddef>

#if defined(CXX_MSVC)
#define PACKED
#else
#define PACKED __attribute__((packed))
#endif

#define UNUSED(x) (void)(x)
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(*x))

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

// --- User-defined literals

constexpr inline u64 operator"" _K(u64 value) { return value * 1000u; }
constexpr inline u64 operator"" _KiB(u64 value) { return value << 10; }
constexpr inline u64 operator"" _MiB(u64 value) { return value << 20; }
constexpr inline u64 operator"" _GiB(u64 value) { return value << 30; }
