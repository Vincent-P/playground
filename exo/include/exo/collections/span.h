#pragma once
#include <exo/macros/assert.h>
#include <exo/maths/numerics.h>
#include <span>

namespace exo
{
template <typename T> using Span = std::span<T>;

template <typename T> Span<T> reinterpret_span(Span<u8> bytes)
{
	ASSERT(bytes.size_bytes() % sizeof(T) == 0);
	return std::span(reinterpret_cast<T *>(bytes.data()), bytes.size_bytes() / sizeof(T));
}
}; // namespace exo
