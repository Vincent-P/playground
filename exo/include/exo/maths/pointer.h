#pragma once
#include "exo/maths/numerics.h"

namespace exo
{
template <typename T>
constexpr T *ptr_offset(T *ptr, usize offset)
{
    return reinterpret_cast<T *>(reinterpret_cast<char *>(ptr) + offset);
}

template <typename T>
constexpr const T *ptr_offset(const T *ptr, usize offset)
{
    return reinterpret_cast<const T *>(reinterpret_cast<const char *>(ptr) + offset);
}

constexpr usize round_up_to_alignment(usize alignment, usize bytes)
{
    const usize mask = alignment - 1;
    return (bytes + mask) & ~mask;
}
} // namespace exo
