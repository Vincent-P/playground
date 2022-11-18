#pragma once
#include "exo/macros/assert.h"
#include "exo/maths/numerics.h"
#include <initializer_list>

namespace exo
{

namespace details
{
template <typename A, typename B>
inline constexpr bool IsSameV = false;

template <typename A>
inline constexpr bool IsSameV<A, A> = true;

template <typename A, typename B>
concept IsSame = IsSameV<A, B>;
}; // namespace details

template <typename T>
struct Span
{
	T    *ptr    = nullptr;
	usize length = 0;

	// --

	Span() = default;
	Span(T *data, usize len) : ptr{data}, length{len} {}
	Span(T *begin, T *end) : ptr{begin}, length(usize(end - begin)) {}

	Span(const Span &other) : Span{other.ptr, other.length} {}

	operator Span<const T>() const { return Span<const T>(this->ptr, this->length); }

	// -- Iterators

	T *begin() const { return ptr; }
	T *end() const { return ptr + length; }

	// -- Element access

	T &operator[](usize i) const
	{
		ASSERT(i < this->length);
		return this->ptr[i];
	}

	T *data() const { return this->ptr; }

	T &back() const { return (*this)[this->length - 1]; }

	// -- Observers

	usize len() const { return this->length; }

	bool empty() const { return this->length == 0; }

	// -- Subviews

	Span subspan(usize offset)
	{
		ASSERT(this->length >= offset);
		return Span{this->ptr + offset, this->length - offset};
	}

	// -- STL compatibility

	usize size_bytes() const { return length * sizeof(T); }
};

template<typename T>
Span(T *, T *) -> Span<T>;

template <typename T>
Span(T *, usize) -> Span<T>;

template <typename T>
Span<T> reinterpret_span(Span<u8> bytes)
{
	ASSERT(bytes.size_bytes() % sizeof(T) == 0);
	return Span(reinterpret_cast<T *>(bytes.data()), bytes.size_bytes() / sizeof(T));
}

template <typename T>
Span<const T> reinterpret_span(Span<const u8> bytes)
{
	ASSERT(bytes.size_bytes() % sizeof(T) == 0);
	return Span(reinterpret_cast<const T *>(bytes.data()), bytes.size_bytes() / sizeof(T));
}

template <typename T>
Span<const u8> span_to_bytes(Span<T> elements)
{
	return Span(reinterpret_cast<const u8 *>(elements.data()), elements.size_bytes());
}
}; // namespace exo
