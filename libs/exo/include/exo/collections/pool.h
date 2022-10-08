#pragma once

#include "exo/collections/handle.h"
#include "exo/collections/iterator_facade.h"
#include "exo/macros/assert.h"
#include "exo/maths/pointer.h"
#include "exo/memory/dynamic_buffer.h"

#include <iterator>

/**
   A Pool is a linear allocator with a free-list.
   Performance:
     Adding/removing elements is O(1).
     Iterating is O(capacity) and elements are NOT tighly packed because of the free-list.

   TODO: Add run length of holes in the metdata to skip big holes when iterating.
 **/

namespace exo
{
// Impl below helpers
template <typename T> struct PoolIterator;

template <typename T> struct ConstPoolIterator;

template <typename T> struct Pool
{
	static constexpr u32 ELEMENT_SIZE() { return sizeof(T) < sizeof(u32) ? sizeof(u32) : sizeof(T); }

	Pool() = default;
	explicit Pool(u32 _capacity);
	~Pool();

	Pool(const Pool &other) = delete;
	Pool &operator=(const Pool &other) = delete;

	Pool(Pool &&other);
	Pool &operator=(Pool &&other);

	Handle<T> add(T &&value);
	const T  &get(Handle<T> handle) const;
	T        &get(Handle<T> handle);
	const T  &get_unchecked(u32 index) const;
	void      remove(Handle<T> handle);
	void      clear();

	PoolIterator<T> begin();
	PoolIterator<T> end();

	ConstPoolIterator<T> begin() const;
	ConstPoolIterator<T> end() const;

	bool operator==(const Pool &rhs) const = default;

	DynamicBuffer buffer        = {};
	u32           freelist_head = u32_invalid;
	u32           size          = 0;
	u32           capacity      = 0;
};

// Helpers
namespace
{
// Each element in the buffer is prepended with metadata
union ElementMetadata
{
	struct
	{
		u32 is_occupied : 1;
		u32 generation : 31;
	} bits;
	u32 raw;
};

template <typename T> T *element_ptr(Pool<T> &pool, u32 i)
{
	return reinterpret_cast<T *>(
		ptr_offset(pool.buffer.ptr, i * (Pool<T>::ELEMENT_SIZE() + sizeof(ElementMetadata)) + sizeof(ElementMetadata)));
}

template <typename T> const T *element_ptr(const Pool<T> &pool, u32 i)
{
	return reinterpret_cast<const T *>(
		ptr_offset(pool.buffer.ptr, i * (Pool<T>::ELEMENT_SIZE() + sizeof(ElementMetadata)) + sizeof(ElementMetadata)));
}

template <typename T> u32 *freelist_ptr(Pool<T> &pool, u32 i) { return reinterpret_cast<u32 *>(element_ptr(pool, i)); }

template <typename T> const u32 *freelist_ptr(const Pool<T> &pool, u32 i)
{
	return reinterpret_cast<const u32 *>(element_ptr(pool, i));
}

template <typename T> ElementMetadata *metadata_ptr(Pool<T> &pool, u32 i)
{
	return reinterpret_cast<ElementMetadata *>(
		ptr_offset(pool.buffer.ptr, i * (Pool<T>::ELEMENT_SIZE() + sizeof(ElementMetadata))));
}

template <typename T> const ElementMetadata *metadata_ptr(const Pool<T> &pool, u32 i)
{
	return reinterpret_cast<const ElementMetadata *>(
		ptr_offset(pool.buffer.ptr, i * (Pool<T>::ELEMENT_SIZE() + sizeof(ElementMetadata))));
}
} // namespace

template <typename T> struct PoolIterator : IteratorFacade<PoolIterator<T>>
{
	PoolIterator() = default;
	PoolIterator(Pool<T> *_pool, u32 _index = 0) : pool{_pool}, current_index{_index} {}

	std::pair<Handle<T>, T *> dereference() const
	{
		auto *metadata = metadata_ptr(*pool, current_index);
		auto *element  = element_ptr(*pool, current_index);

		Handle<T> handle = {current_index, metadata->bits.generation};
		return std::make_pair(handle, element);
	}

	void increment()
	{
		for (current_index = current_index + 1; current_index < pool->capacity; current_index += 1) {
			auto *metadata = metadata_ptr(*pool, current_index);
			auto *element  = element_ptr(*pool, current_index);
			if (metadata->bits.is_occupied == 1) {
				break;
			}
		}
	}

	bool equal_to(const PoolIterator &other) const
	{
		return pool == other.pool && current_index == other.current_index;
	}

	Pool<T> *pool          = nullptr;
	u32      current_index = u32_invalid;
};

template <typename T> struct ConstPoolIterator : IteratorFacade<ConstPoolIterator<T>>
{
	ConstPoolIterator() = default;
	ConstPoolIterator(const Pool<T> *_pool, u32 _index = 0) : pool{_pool}, current_index{_index} {}

	std::pair<Handle<T>, const T *> dereference() const
	{
		const auto *metadata = metadata_ptr(*pool, current_index);
		const auto *element  = element_ptr(*pool, current_index);

		Handle<T> handle = {current_index, metadata->bits.generation};
		return std::make_pair(handle, element);
	}

	void increment()
	{
		for (current_index = current_index + 1; current_index < pool->capacity; current_index += 1) {
			auto *metadata = metadata_ptr(*pool, current_index);
			auto *element  = element_ptr(*pool, current_index);
			if (metadata->bits.is_occupied == 1) {
				break;
			}
		}
	}

	bool equal_to(const ConstPoolIterator &other) const
	{
		return pool == other.pool && current_index == other.current_index;
	}

	const Pool<T> *pool          = nullptr;
	u32            current_index = u32_invalid;
};

template <typename T> Pool<T>::Pool(u32 _capacity)
{
	capacity = _capacity;

	if (capacity == 0) {
		return;
	}

	usize buffer_size = capacity * (Pool<T>::ELEMENT_SIZE() + sizeof(ElementMetadata));
	DynamicBuffer::init(this->buffer, buffer_size);

	// Init the free list
	freelist_head = 0;
	for (u32 i = 0; i < capacity - 1; i += 1) {
		auto *metadata = metadata_ptr(*this, i);
		metadata->raw  = 0;

		u32 *freelist_element = freelist_ptr(*this, i);
		*freelist_element     = i + 1;
	}

	metadata_ptr(*this, capacity - 1)->raw = 0;
	*freelist_ptr(*this, capacity - 1)     = u32_invalid;
}

template <typename T> Pool<T>::~Pool() { this->buffer.destroy(); }

template <typename T> Pool<T>::Pool(Pool &&other) { *this = std::move(other); }

template <typename T> Pool<T> &Pool<T>::operator=(Pool &&other)
{
	this->buffer        = std::exchange(other.buffer, {});
	this->freelist_head = std::exchange(other.freelist_head, u32_invalid);
	this->size          = std::exchange(other.size, 0);
	this->capacity      = std::exchange(other.capacity, 0);
	return *this;
}

template <typename T> Handle<T> Pool<T>::add(T &&value)
{
	// Realloc memory if the container is full
	if (freelist_head == u32_invalid) {
		ASSERT(size + 1 >= capacity);

		auto new_capacity = capacity * 2;
		if (new_capacity == 0) {
			new_capacity = 2;
		}

		usize new_size = new_capacity * (Pool<T>::ELEMENT_SIZE() + sizeof(ElementMetadata));
		this->buffer.resize(new_size);

		// extend the freelist
		freelist_head = capacity;
		for (u32 i = capacity; i < new_capacity - 1; i += 1) {
			auto *metadata = metadata_ptr(*this, i);
			metadata->raw  = 0;

			u32 *freelist_element = freelist_ptr(*this, i);
			*freelist_element     = i + 1;
		}

		metadata_ptr(*this, new_capacity - 1)->raw = 0;
		*freelist_ptr(*this, new_capacity - 1)     = u32_invalid;

		capacity = new_capacity;
	}

	ASSERT(size + 1 <= capacity);

	// Pop the free list head to find an empty place for the new element
	u32 i_element = freelist_head;
	freelist_head = *freelist_ptr(*this, i_element);

	// Construct the new element
	T    *element  = new (element_ptr(*this, i_element)) T{std::forward<T>(value)};
	auto *metadata = metadata_ptr(*this, i_element);
	ASSERT(metadata->bits.is_occupied == 0);
	metadata->bits.is_occupied = 1;

	size += 1;

	return {i_element, metadata->bits.generation};
}

template <typename T> const T &Pool<T>::get(Handle<T> handle) const
{
	ASSERT(handle.is_valid());

	u32   i_element = handle.index;
	auto *metadata  = metadata_ptr(*this, i_element);
	auto *element   = element_ptr(*this, i_element);

	ASSERT(i_element < capacity);
	ASSERT(metadata->bits.is_occupied);
	ASSERT(metadata->bits.generation == handle.gen);

	return *element;
}

template <typename T> const T &Pool<T>::get_unchecked(u32 index) const { return *element_ptr(*this, index); }

template <typename T> T &Pool<T>::get(Handle<T> handle)
{
	return const_cast<T &>(static_cast<const Pool<T> &>(*this).get(handle));
}

template <typename T> void Pool<T>::remove(Handle<T> handle)
{
	auto *metadata = metadata_ptr(*this, handle.index);
	auto *element  = element_ptr(*this, handle.index);
	auto *freelist = freelist_ptr(*this, handle.index);

	ASSERT(metadata->bits.generation == handle.gen);
	ASSERT(metadata->bits.is_occupied == 1);

	// Destruct the element
	element->~T();
	metadata->bits.generation  = metadata->bits.generation + 1;
	metadata->bits.is_occupied = 0;

	// Push this slot to the head of the free list
	*freelist     = freelist_head;
	freelist_head = handle.index;

	size = size - 1;
}

template <typename T> void Pool<T>::clear()
{
	this->size          = 0;
	this->freelist_head = 0;

	for (u32 i = 0; i < this->capacity; i += 1) {
		auto *metadata = metadata_ptr(*this, i);

		if (metadata->bits.is_occupied) {
			auto *p_element = element_ptr(*this, i);
			p_element->~T();
		}

		metadata->raw = 0;

		u32 *freelist_element = freelist_ptr(*this, i);
		*freelist_element     = i + 1;
	}

	*freelist_ptr(*this, this->capacity - 1) = u32_invalid;
}

template <typename T> PoolIterator<T> Pool<T>::begin()
{
	u32 i_element = 0;
	for (; i_element < capacity; i_element += 1) {
		auto *metadata = metadata_ptr(*this, i_element);
		if (metadata->bits.is_occupied) {
			break;
		}
	}
	return PoolIterator<T>(this, i_element);
}

template <typename T> PoolIterator<T> Pool<T>::end() { return PoolIterator<T>(this, capacity); }

template <typename T> ConstPoolIterator<T> Pool<T>::begin() const
{
	u32 i_element = 0;
	for (; i_element < capacity; i_element += 1) {
		auto *metadata = metadata_ptr(*this, i_element);
		if (metadata->bits.is_occupied) {
			break;
		}
	}
	return ConstPoolIterator<T>(this, i_element);
}

template <typename T> ConstPoolIterator<T> Pool<T>::end() const { return ConstPoolIterator<T>(this, capacity); }
} // namespace exo
