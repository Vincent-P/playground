#pragma once
#include "exo/maths/numerics.h"

#include <type_traits>

namespace exo
{
template <typename T>
[[nodiscard]] inline u64 hash_combine(u64 seed, const T &v)
{
	seed ^= hash_value(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	return seed;
}

template <>
[[nodiscard]] inline u64 hash_combine<u64>(u64 seed, const u64 &hash)
{
	seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	return seed;
}

[[nodiscard]] inline u64 hash_value(void const *const ptr)
{
	u64 seed = 0;
	seed     = hash_combine(seed, u64(ptr));
	return seed;
}

} // namespace exo
