#pragma once
#include <exo/maths/numerics.h>

namespace exo::Array
{
template <typename T, usize N> using Reference = T (&)[N];
template <typename T, usize N> [[nodiscard]] inline constexpr usize len(Reference<const T, N> _) { return N; }
} // namespace exo::Array
