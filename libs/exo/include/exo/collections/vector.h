#pragma once
#include <exo/collections/span.h>
#include <exo/macros/assert.h>
#include <exo/maths/numerics.h>
#include <exo/memory/dynamic_buffer.h>

#include <initializer_list>
#include <type_traits>
#include <utility>

namespace exo
{
template <typename T>
struct Vec
{
	DynamicBuffer buffer = {};
	usize         length = 0;

	// --
	Vec() = default;
	Vec(std::initializer_list<T> values)
	{
		this->reserve(values.size());
		for (auto &val : values) {
			this->_push_noreserve(val);
		}
	}

	static Vec with_capacity(usize capacity)
	{
		Vec result = {};
		result.reserve(capacity);
		return result;
	}

	static Vec with_length(usize length)
	{
		Vec result = {};
		result.resize(length);
		return result;
	}

	static Vec with_values(usize length, const T &value)
	{
		Vec result = {};
		result.resize(length, value);
		return result;
	}

	// Element access

	T &operator[](usize i)
	{
		ASSERT(i < length);
		const auto values = exo::reinterpret_span<T>(this->buffer.content());
		return values[i];
	}

	const T &operator[](usize i) const
	{
		ASSERT(i < length);
		const auto values = exo::reinterpret_span<const T>(this->buffer.content());
		return values[i];
	}

	T       &last() { return (*this)[this->length - 1]; }
	const T &last() const { return (*this)[this->length - 1]; }

	T       *data() { return static_cast<T *>(this->buffer.ptr); }
	const T *data() const { return static_cast<const T *>(this->buffer.ptr); }

	// Iterators

	T *begin() { return &this->data()[0]; }
	T *end() { return &this->data()[this->length]; }

	const T *begin() const { return &this->data()[0]; }
	const T *end() const { return &this->data()[this->length]; }

	// Capacity

	bool is_empty() const { return this->length == 0; }

	usize len() const { return this->length; }

	void reserve(usize new_capacity)
	{
		usize capacity_bytes     = this->buffer.size;
		usize new_capacity_bytes = new_capacity * sizeof(T);
		if (new_capacity_bytes > capacity_bytes) {
			auto          old_buffer = std::move(this->buffer);
			DynamicBuffer new_buffer = {};
			DynamicBuffer::init(new_buffer, new_capacity_bytes);

			const auto old_values = exo::reinterpret_span<T>(old_buffer.content());
			const auto new_values = exo::reinterpret_span<T>(new_buffer.content());

			for (usize i = 0; i < this->length; ++i) {
				if constexpr (std::is_move_constructible_v<T>) {
					new (&new_values[i]) T(std::move(old_values[i]));
				} else {
					new_values[i] = old_values[i];
				}
			}

			old_buffer.destroy();
			this->buffer = std::move(new_buffer);
		}
	}

	usize capacity() const { return this->buffer.size / sizeof(T); }

	// Modifiers

	void clear()
	{
		const auto values = exo::reinterpret_span<T>(this->buffer.content());
		for (usize i = 0; i < this->length; ++i) {
			values[i].~T();
		}
		this->length = 0;
	}

	template <typename... Args>
	T &_push_noreserve(Args &&...args)
	{
		const auto values = exo::reinterpret_span<T>(this->buffer.content());
		new (&values[this->length]) T(std::forward<Args>(args)...);
		this->length += 1;
		return values[this->length - 1];
	}

	template <typename... Args>
	T &push(Args &&...args)
	{
		const auto capacity = this->capacity();
		if (this->length + 1 > capacity) [[unlikely]] {
			const auto new_capacity = capacity > 0 ? 2 * capacity : 2;
			this->reserve(new_capacity);
		}

		return this->_push_noreserve<Args...>(std::forward<Args>(args)...);
	}

	T pop()
	{
		ASSERT(this->length > 0);
		const auto values = exo::reinterpret_span<T>(this->buffer.content());
		this->length -= 1;
		return std::move(values[this->length]);
	}

	void resize(usize new_length)
	{
		if (new_length < this->length) {
			const auto values = exo::reinterpret_span<T>(this->buffer.content());
			for (usize i = new_length; i < this->length; ++i) {
				values[i].~T();
			}
		} else if (new_length > this->length) {
			this->reserve(new_length);
			const auto values = exo::reinterpret_span<T>(this->buffer.content());
			for (usize i = length; i < new_length; ++i) {
				new (&values[i]) T();
			}
		}
		this->length = new_length;
	}

	void resize(usize new_length, const T &value)
	{
		if (new_length < this->length) {
			const auto values = exo::reinterpret_span<T>(this->buffer.content());
			for (usize i = new_length; i < this->length; ++i) {
				values[i].~T();
			}
		} else if (new_length > this->length) {
			this->reserve(new_length);
			const auto values = exo::reinterpret_span<T>(this->buffer.content());
			for (usize i = length; i < new_length; ++i) {
				new (&values[i]) T(value);
			}
		}
		this->length = new_length;
	}

	void swap_remove(usize i)
	{
		ASSERT(i < this->length);
		const auto values = exo::reinterpret_span<T>(this->buffer.content());
		if (this->length > 1 && i < this->length - 1) {
			std::swap(values[i], values[this->length - 1]);
		}
		this->length -= 1;
		values[this->length].~T();
	}
};
} // namespace exo

using exo::Vec;
