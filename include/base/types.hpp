#pragma once
#include <algorithm>
#include <cstddef>
#include <glm/glm.hpp>

#define ARRAY_SIZE(_arr) (sizeof(_arr) / sizeof(*_arr))

#define MEMBER_OFFSET(type, member) (static_cast<u32>(reinterpret_cast<u64>(&reinterpret_cast<type *>(0)->member)))

#define not_implemented()                                                                                              \
    {                                                                                                                  \
        assert(false);                                                                                                 \
    }

#define PACKED __attribute__((packed))

/// --- Constants

constexpr float PI = 3.1415926535897932384626433832795f;

/// --- Numeric Types

using i8    = std::int8_t;
using i16   = std::int16_t;
using i32   = std::int32_t;
using i64   = std::int64_t;
using u8    = std::uint8_t;
using u16   = std::uint16_t;
using u32   = std::uint32_t;
using u64   = std::uint64_t;
using usize = std::size_t;
using uchar = unsigned char;
using uint  = unsigned int;

static constexpr u32 u32_invalid = ~0u;

/// --- Vector types

using float2   = glm::vec2;
using float3   = glm::vec3;
using float4   = glm::vec4;
using int2     = glm::ivec2;
using int3     = glm::ivec3;
using int4     = glm::ivec4;
using uint2     = glm::uvec2;
using uint3     = glm::uvec3;
using uint4     = glm::uvec4;
using float4x4 = glm::mat4;


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
