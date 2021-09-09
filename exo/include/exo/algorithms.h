#pragma once
#include "exo/prelude.h"

#include <execution>
#include <iterator>

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

// Fallback for compiling with cl.exe
#if defined(_MSC_VER) && !defined(__clang_major__)

template <typename S, typename D, typename L>
inline void map_transform(const S &src, D &dst, L lambda)
{
    dst.reserve(src.size());
    std::transform(src.begin(), src.end(), std::back_inserter(dst), lambda);
}

template <typename S, typename L>
inline void parallel_foreach(S &container, L lambda)
{
    std::for_each(std::execution::par_unseq, std::begin(container), std::end(container), lambda);
}

#else

inline void map_transform(const auto &src, auto &dst, auto lambda)
{
    dst.reserve(src.size());
    std::transform(src.begin(), src.end(), std::back_inserter(dst), lambda);
}

inline void parallel_foreach(auto &container, auto lambda)
{
    std::for_each(std::execution::par_unseq, std::begin(container), std::end(container), lambda);
}

#endif
