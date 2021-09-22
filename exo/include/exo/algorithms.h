#pragma once
#include "exo/prelude.h"

#include <execution>
#include <iterator>

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
