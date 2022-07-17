#pragma once
#include "exo/maths/numerics.h"

namespace exo
{
/// --- Handle type (Typed index that can be invalid)
usize _hash_handle(u32 index, u32 gen);

template <typename T> struct Handle
{
	static constexpr Handle invalid() { return Handle(); }
	constexpr Handle() : Handle(u32_invalid, u32_invalid) {}

	constexpr Handle &operator=(const Handle &other)        = default;
	constexpr bool    operator==(const Handle &other) const = default;

	[[nodiscard]] constexpr u32  value() const { return index; }
	[[nodiscard]] constexpr bool is_valid() const { return index != u32_invalid && gen != u32_invalid; }

private:
	constexpr Handle(u32 _index, u32 _gen) : index(_index), gen(_gen) {}

	u32 index;
	u32 gen;

	template <typename T> friend struct Pool;
	template <typename T> friend struct PoolIterator;
	template <typename T> friend struct ConstPoolIterator;
	friend inline usize hash_value(const Handle<T> &h) { return _hash_handle(h.index, h.gen); }
};
} // namespace exo

using exo::Handle;
