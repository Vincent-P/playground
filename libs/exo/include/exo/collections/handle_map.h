#pragma once
#include "exo/collections/handle.h"
#include <bit>

// Utilities for using Handle<T> in an exo::IndexMap.
// Should NOT be included in a header!
namespace exo
{
template <typename T> Handle<T> as_handle(u64 bytes)
{
	static_assert(sizeof(Handle<T>) == sizeof(u64));
	return std::bit_cast<Handle<T>>(bytes);
}

template <typename T> u64 to_u64(Handle<T> handle)
{
	static_assert(sizeof(Handle<T>) == sizeof(u64));
	return std::bit_cast<u64>(handle);
}
} // namespace exo
