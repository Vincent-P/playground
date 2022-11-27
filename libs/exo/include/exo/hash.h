#pragma once
#include "exo/maths/numerics.h"

namespace exo
{
[[nodiscard]] inline u64 hash_combine(u64 seed, u64 hash)
{
	seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	return seed;
}

[[nodiscard]] inline u64 hash_value(const void *const ptr)
{
	u64 seed = 0;
	seed = hash_combine(seed, u64(ptr));
	return seed;
}

struct RawHash
{
	u64 value = 0;
	RawHash() = default;
	RawHash(u64 new_value) : value{new_value} {}

	bool operator==(const RawHash &other) const { return this->value == other.value; }
	bool operator!=(const RawHash &other) const { return this->value != other.value; }
};

[[nodiscard]] inline u64 hash_value(RawHash raw_hash) { return raw_hash.value; }
} // namespace exo
