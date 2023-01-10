#pragma once
#include "exo/macros/assert.h"
#include "exo/maths/numerics.h"

namespace exo
{
template <typename Enum>
concept EnumCount = requires(Enum e) { Enum::Count; };

template <typename T, EnumCount E>
struct EnumArray
{
	static constexpr usize LENGTH = static_cast<usize>(E::Count);

	// Note that the member data is intentionally public.
	// This allows for aggregate initialization of the
	// object (e.g. EnumArray<int, IntEnum> a = { 0, 3, 2, 4 }; )
	T array[LENGTH];

	constexpr const T &operator[](E e) const
	{
		ASSERT(static_cast<usize>(e) < LENGTH);
		return array[static_cast<usize>(e)];
	}
	constexpr T &operator[](E e)
	{
		ASSERT(static_cast<usize>(e) < LENGTH);
		return array[static_cast<usize>(e)];
	}

	constexpr const T *begin() const
	{
		return &array[0];
	}

	constexpr T *begin()
	{
		return &array[0];
	}

	constexpr const T *end() const
	{
		return &array[LENGTH];
	}

	constexpr T *end()
	{
		return &array[LENGTH];
	}
};
} // namespace exo
