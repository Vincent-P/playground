#pragma once
#include "exo/maths/numerics.h"

#include <type_traits>

namespace exo
{
[[nodiscard]] inline u64 hash_combine(u64 seed, u64 hash)
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

[[nodiscard]] inline u64 hash_value(u64 raw_hash) { return raw_hash; }
} // namespace exo
