#pragma once
#include "exo/maths/numerics.h"

namespace exo
{
template <typename T> u64 hash_value(const T &v) { return v.hash_value(); }

template <typename T> inline u64 hash_combine(u64 seed, const T &v)
{
	seed ^= hash_value(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	return seed;
}

template <> inline u64 hash_combine<u64>(u64 seed, const u64 &hash)
{
	seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	return seed;
}
} // namespace exo
