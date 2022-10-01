#pragma once

namespace exo
{
template <typename ForwardedType, size_t MAX_SIZE = 4 * sizeof(void *)> struct ForwardContainer
{
	const ForwardedType &get() const
	{
		static_assert(sizeof(ForwardedType) <= MAX_SIZE);
		return *reinterpret_cast<const ForwardedType *>(&bytes[0]);
	}

	ForwardedType &get()
	{
		static_assert(sizeof(ForwardedType) <= MAX_SIZE);
		return *reinterpret_cast<ForwardedType *>(&bytes[0]);
	}

	unsigned char bytes[MAX_SIZE];
};
}; // namespace exo
