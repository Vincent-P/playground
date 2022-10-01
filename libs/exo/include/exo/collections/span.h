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

template <typename T> Span<const u8> span_to_bytes(Span<const T> elements)
{
	return std::span(reinterpret_cast<const u8 *>(elements.data()), elements.size_bytes());
}
}; // namespace exo
