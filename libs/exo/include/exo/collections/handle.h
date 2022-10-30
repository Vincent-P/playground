#pragma once
#include "exo/maths/numerics.h"

namespace exo
{
struct Serializer;

/// --- Handle type (Typed index that can be invalid)
usize _hash_handle(u32 index, u32 gen);

template <typename T>
struct Handle
{
	static constexpr Handle invalid() { return Handle(); }
	constexpr Handle() noexcept : Handle(u32_invalid, u32_invalid) {}

	constexpr Handle &operator=(const Handle &other)        = default;
	constexpr bool    operator==(const Handle &other) const = default;

	[[nodiscard]] constexpr bool is_valid() const { return index != u32_invalid && gen != u32_invalid; }
	[[nodiscard]] constexpr u32  get_index() const { return index; }

private:
	constexpr Handle(u32 _index, u32 _gen) noexcept : index(_index), gen(_gen) {}

	u32 index;
	u32 gen;

	template <typename T>
	friend struct Pool;
	template <typename T>
	friend struct PoolIterator;
	template <typename T>
	friend struct ConstPoolIterator;

	friend inline u64 hash_value(const Handle<T> &h) { return _hash_handle(h.index, h.gen); }

	template <typename K>
	friend void serialize(Serializer &serializer, Handle<K> &value);
};

} // namespace exo

using exo::Handle;
