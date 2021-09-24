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

#include <cstddef>


/// --- Portable compiler attribute/builtins

#if defined(CXX_MSVC)
#define PACKED
#else
#define PACKED __attribute__((packed))
#endif

namespace exo
{
#if defined(CXX_MSVC)
[[noreturn]] __forceinline void unreachable() {__assume(0);}
#else
[[noreturn]] inline __attribute__((always_inline)) void unreachable() {__builtin_unreachable();}
#endif
}

// --- Useful macros/functions

#define UNUSED(x) (void)(x)
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(*x))
#undef assert
void internal_assert(bool condition, const char *condition_str);

#define STR(x) #x
#define ASSERT(x) internal_assert(x, STR(x))

template <typename T>
inline T *ptr_offset(T *ptr, usize offset)
{
    return reinterpret_cast<T *>(reinterpret_cast<char *>(ptr) + offset);
}

template <typename T>
inline const T *ptr_offset(const T *ptr, usize offset)
{
    return reinterpret_cast<const T *>(reinterpret_cast<const char *>(ptr) + offset);
}

inline usize round_up_to_alignment(usize alignment, usize bytes)
{
    const usize mask = alignment - 1;
    return (bytes + mask) & ~mask;
}

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
