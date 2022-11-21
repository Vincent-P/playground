#pragma once
#include "exo/collections/span.h"
#include "exo/macros/assert.h"
#include "exo/maths/numerics.h"

#include <initializer_list>
#include <type_traits>
#include <utility>

namespace exo
{
template <typename T, usize CAPACITY>
struct DynamicArray
{
	T     values[CAPACITY] = {};
	usize length           = {0};

	// --
	constexpr DynamicArray() = default;

	constexpr DynamicArray(exo::Span<const T> span)
	{
		ASSERT(span.len() < CAPACITY);
		length = span.len();
		for (usize i = 0; i < length; i += 1) {
			this->values[i] = span[i];
		}
	}

	constexpr DynamicArray(std::initializer_list<T> list)
	{
		ASSERT(list.size() < CAPACITY);
		for (auto &value : list) {
			this->values[this->length++] = value;
		}
	}

	constexpr DynamicArray(const DynamicArray &other) { *this = other; }
	constexpr DynamicArray &operator=(const DynamicArray &other)
	{
		for (u32 i = 0; i < other.length; ++i) {
			this->values[i] = other.values[i];
		}
		this->length = other.length;
		return *this;
	}

	constexpr DynamicArray(DynamicArray &&other) { *this = std::move(other); }
	constexpr DynamicArray &operator=(DynamicArray &&other)
	{
		for (u32 i = 0; i < other.length; ++i) {
			this->values[i] = std::move(other.values[i]);
		}
		this->length = other.length;
		other.length = 0;
		return *this;
	}

	constexpr ~DynamicArray()
	{
		if constexpr (std::is_trivially_destructible<T>() == false) {
			for (u32 i = 0; i < length; ++i) {
				this->values[i].~T();
			}
		}
		this->length = 0;
	}

	operator Span<T>() { return Span<T>(this->values, this->length); }
	operator Span<const T>() const { return Span<const T>(this->values, this->length); }

	// Element access

	constexpr const T &operator[](usize i) const
	{
		ASSERT(i < this->length);
		return this->values[i];
	}

	constexpr T &operator[](usize i)
	{
		ASSERT(i < this->length);
		return this->values[i];
	}

	constexpr const T *data() const noexcept { return &this->values[0]; }
	constexpr T       *data() noexcept { return &this->values[0]; }

	constexpr T       &last() noexcept { return this->values[this->length - 1]; }
	constexpr const T &last() const noexcept { return this->values[this->length - 1]; }

	// Iterators

	constexpr const T *begin() const noexcept { return &this->values[0]; }
	constexpr T       *begin() noexcept { return &this->values[0]; }

	constexpr const T *end() const noexcept { return begin() + this->length; }
	constexpr T       *end() noexcept { return begin() + this->length; }

	// Capacity

	constexpr bool  is_empty() const noexcept { return this->length == 0; }
	constexpr usize len() const noexcept { return this->length; }
	constexpr usize capacity() const noexcept { return CAPACITY; }

	// Modifiers
	template <typename... Args>
	T &push(Args &&...args)
	{
		ASSERT(this->length + 1 <= CAPACITY);
		auto *pushed_value = new (&this->values[this->length]) T(std::forward<Args>(args)...);
		this->length += 1;
		return *pushed_value;
	}

	T pop()
	{
		ASSERT(this->length > 0);
		this->length -= 1;
		return std::move(this->values[this->length]);
	}

	constexpr void clear() noexcept
	{
		this->length = 0;

		if constexpr (std::is_trivially_destructible<T>() == false) {
			for (usize i = 0; i < this->length; i += 1) {
				this->values[i].~T();
			}
		}
	}
	constexpr void resize(usize new_size) noexcept
	{
		ASSERT(new_size <= CAPACITY);

		// Default-initialize new elements if new_size > old_size
		for (usize i = this->length; i < new_size; i += 1) {
			this->values[i] = {};
		}

		this->length = new_size;
	}
};

template <typename T, usize C1, usize C2>
constexpr bool operator==(const DynamicArray<T, C1> &lhs, const DynamicArray<T, C2> &rhs)
{
	if (lhs.length != rhs.length) {
		return false;
	}

	for (usize i = 0; i < lhs.length; ++i) {
		if (lhs[i] != rhs[i]) {
			return false;
		}
	}

	return true;
}
} // namespace exo
