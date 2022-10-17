#pragma once
#include <exo/collections/map.h>

namespace exo
{
inline constexpr u32 EXO_SET_MAX_LOAD_FACTOR_NOM   = 3;
inline constexpr u32 EXO_SET_MAX_LOAD_FACTOR_DENOM = 4;

template <typename T>
struct SetIterator;

template <typename T>
struct SetConstIterator;

/**
   The exo::Set is a "flat" hashset, implemented with open addressing to have contigous memory allocation.
   It uses linear probing with robin-hood hashing.
**/
template <typename T>
struct Set
{
	u32           capacity      = 0;
	u32           size          = 0;
	DynamicBuffer values_buffer = {};
	DynamicBuffer slots_buffer  = {};

	static Set with_capacity(u32 new_capacity);
	inline ~Set()
	{
		this->values_buffer.destroy();
		this->slots_buffer.destroy();
	}

	Set() = default;

	Set(const Set &copy)            = delete;
	Set &operator=(const Set &copy) = delete;

	inline Set(Set &&moved) noexcept { *this = std::move(moved); }
	inline Set &operator=(Set &&moved) noexcept
	{
		this->capacity      = moved.capacity;
		this->size          = moved.size;
		this->values_buffer = std::move(moved.values_buffer);
		this->slots_buffer  = std::move(moved.slots_buffer);
		return *this;
	}

	// iterators
	SetIterator<T> begin() { return SetIterator<T>(this); }
	SetIterator<T> end() { return SetIterator<T>(this, this->capacity); }

	SetConstIterator<T> begin() const { return SetConstIterator<T>(this); }
	SetConstIterator<T> end() const { return SetConstIterator<T>(this, this->capacity); }

	bool contains(const T &value);
	T   *insert(T &&value);
	T   *insert(const T &value);
	void remove(const T &value);
};

template <typename T>
Set<T> Set<T>::with_capacity(u32 new_capacity)
{
	ASSERT(std::has_single_bit(new_capacity));

	Set set      = {};
	set.capacity = new_capacity;
	DynamicBuffer::init(set.values_buffer, new_capacity * sizeof(T));
	DynamicBuffer::init(set.slots_buffer, new_capacity * sizeof(details::MapSlot));
	return set;
}

template <typename T>
bool Set<T>::contains(const T &value)
{
	if (this->size == 0) {
		return false;
	}

	const auto slots  = exo::reinterpret_span<details::MapSlot>(this->slots_buffer.content());
	const auto hash   = hash_value(value);
	u32        i_slot = details::probe_by_hash(slots, hash);

	return i_slot != u32_invalid;
}

template <typename T>
T *Set<T>::insert(T &&value)
{
	auto max_load_size = (this->capacity * EXO_SET_MAX_LOAD_FACTOR_NOM) / EXO_SET_MAX_LOAD_FACTOR_DENOM;
	if (this->size + 1 > max_load_size) [[unlikely]] {
		details::resize_and_rehash<T>(this->slots_buffer, this->values_buffer, this->capacity);
	}

	const auto slots  = exo::reinterpret_span<details::MapSlot>(this->slots_buffer.content());
	const auto values = exo::reinterpret_span<T>(this->values_buffer.content());

	details::MapSlot slot_to_insert;
	slot_to_insert.bits.is_filled = 1;
	slot_to_insert.bits.psl       = 0;
	slot_to_insert.bits.hash      = hash_value(value);
	u32 i_slot                    = details::insert_slot(slots, values, std::move(slot_to_insert), std::move(value));

	ASSERT(i_slot < this->capacity);
	this->size += 1;
	return &values[i_slot].value;
}

template <typename T>
T *Set<T>::insert(const T &value)
{
	auto max_load_size = (this->capacity * EXO_SET_MAX_LOAD_FACTOR_NOM) / EXO_SET_MAX_LOAD_FACTOR_DENOM;
	if (this->size + 1 > max_load_size) [[unlikely]] {
		details::resize_and_rehash<T>(this->slots_buffer, this->values_buffer, this->capacity);
	}

	const auto slots  = exo::reinterpret_span<details::MapSlot>(this->slots_buffer.content());
	const auto values = exo::reinterpret_span<T>(this->values_buffer.content());

	details::MapSlot slot_to_insert;
	slot_to_insert.bits.is_filled = 1;
	slot_to_insert.bits.psl       = 0;
	slot_to_insert.bits.hash      = hash_value(value);
	u32 i_slot                    = details::insert_slot(slots, values, std::move(slot_to_insert), T{value});

	ASSERT(i_slot < this->capacity);
	this->size += 1;
	return &values[i_slot];
}

template <typename T>
void Set<T>::remove(const T &value)
{
	const auto slots = exo::reinterpret_span<details::MapSlot>(this->slots_buffer.content());
	const auto hash  = hash_value(value);

	const u32 i_slot = details::probe_by_hash(slots, hash);

	// Not found
	if (i_slot == u32_invalid) {
		ASSERT(false);
		return;
	}

	const auto values = exo::reinterpret_span<T>(this->values_buffer.content());

	// The key was found at slot i_slot, remove it and backward shift all values to fill the hole
	for (u32 i = 0; i < this->capacity; ++i) {
		const auto current_slot = details::power_of_2_modulo((i_slot + i), this->capacity);
		const auto next_slot    = details::power_of_2_modulo((i_slot + i + 1), this->capacity);

		if (slots[next_slot].bits.is_filled == 0 || slots[next_slot].bits.psl == 0) {
			// All elements are shifted towards 0, so whenever we break, the current_slot was already copied to the
			// previous_slot
			slots[current_slot] = {};
			values[current_slot].~T();
			break;
		}

		slots[current_slot] = slots[next_slot];
		ASSERT(slots[current_slot].bits.psl != 0);
		slots[current_slot].bits.psl -= 1;
		values[current_slot] = std::move(values[next_slot]);
	}

	this->size -= 1;
}

// -- Iterators
template <typename T>
struct SetIterator : IteratorFacade<SetIterator<T>>
{
	SetIterator() = default;
	SetIterator(Set<T> *_Set, u32 _index = 0) : set{_Set}, current_index{_index}
	{
		const auto slots = exo::reinterpret_span<details::MapSlot>(this->set->slots_buffer.content());
		if (this->current_index < this->set->capacity && slots[this->current_index].bits.is_filled == 0) {
			this->increment();
		}
	}

	T &dereference() const
	{
		const auto values = exo::reinterpret_span<T>(this->set->values_buffer.content());
		return values[this->current_index];
	}

	void increment()
	{
		const auto slots  = exo::reinterpret_span<details::MapSlot>(this->set->slots_buffer.content());
		const auto values = exo::reinterpret_span<T>(this->set->values_buffer.content());

		for (current_index = current_index + 1; current_index < this->set->capacity; current_index += 1) {
			if (slots[current_index].bits.is_filled == 1) {
				break;
			}
		}
	}

	bool equal_to(const SetIterator &other) const
	{
		return this->set == other.set && this->current_index == other.current_index;
	}

	Set<T> *set           = nullptr;
	u32     current_index = u32_invalid;
};

template <typename T>
struct SetConstIterator : IteratorFacade<SetIterator<T>>
{
	SetConstIterator() = default;
	SetConstIterator(const Set<T> *_Set, u32 _index = 0) : set{_Set}, current_index{_index}
	{
		const auto slots = exo::reinterpret_span<const details::MapSlot>(this->set->slots_buffer.content());
		if (this->current_index < this->set->capacity && slots[this->current_index].bits.is_filled == 0) {
			this->increment();
		}
	}

	const T &dereference() const
	{
		const auto values = exo::reinterpret_span<T>(this->set->values_buffer.content());
		return values[this->current_index];
	}

	void increment()
	{
		const auto slots = exo::reinterpret_span<details::MapSlot>(this->set->slots_buffer.content());

		for (current_index = current_index + 1; current_index < this->set->capacity; current_index += 1) {
			if (slots[current_index].bits.is_filled == 1) {
				break;
			}
		}
	}

	bool equal_to(const SetConstIterator<T> &other) const
	{
		return this->set == other.set && this->current_index == other.current_index;
	}

	const Set<T> *set           = nullptr;
	u32           current_index = u32_invalid;
};

} // namespace exo
