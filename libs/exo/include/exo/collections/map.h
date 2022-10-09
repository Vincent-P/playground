#pragma once
#include <exo/hash.h>
#include <exo/macros/assert.h>
#include <exo/maths/numerics.h>
#include <exo/memory/dynamic_buffer.h>

#include <bit>
#include <span>

namespace exo
{
inline constexpr u32 EXO_MAP_MAX_LOAD_FACTOR_NOM   = 3;
inline constexpr u32 EXO_MAP_MAX_LOAD_FACTOR_DENOM = 4;

union Slot
{
	struct
	{
		u32 is_filled : 1;
		u32 psl : 31; // probe sequence length, iterations needed to lookup element
		u32 hash;
	} bits;
	u64 raw;
};

/**
   The exo::Map is a "flat" hashmap, implemented with open addressing to have contigous memory allocation.
   It uses linear probing with robin-hood hashing.
**/
template <typename Key, typename Value>
struct Map
{
	struct KeyValue
	{
		Key   key;
		Value value;
	};

	u32           capacity         = 0;
	u32           size             = 0;
	DynamicBuffer keyvalues_buffer = {};
	DynamicBuffer slots_buffer     = {};

	static Map with_capacity(u32 new_capacity);
	~Map();

	Value *at(const Key &key);
	Value *insert(Key key, Value &&value);
	void   remove(const Key &key);

private:
	void _resize_and_rehash();
	u32  _insert_slot(Slot &slot_to_insert, KeyValue &value_to_insert);
};

// "Fast" modulo, only works with power of 2 divisors
template <typename Unsigned>
inline constexpr Unsigned power_of_2_modulo(Unsigned a, Unsigned b)
{
	ASSERT(std::has_single_bit(b));
	return a & (b - 1);
}

template <typename Key, typename Value>
Map<Key, Value> Map<Key, Value>::with_capacity(u32 new_capacity)
{
	ASSERT(std::has_single_bit(new_capacity));

	Map map      = {};
	map.capacity = new_capacity;
	DynamicBuffer::init(map.keyvalues_buffer, new_capacity * sizeof(Map::KeyValue));
	DynamicBuffer::init(map.slots_buffers, new_capacity * sizeof(Slot));
	return map;
}

template <typename Key, typename Value>
Map<Key, Value>::~Map()
{
	this->keyvalues_buffer.destroy();
	this->slots_buffer.destroy();
}

template <typename Key, typename Value>
Value *Map<Key, Value>::at(const Key &key)
{
	auto slots     = std::span<Slot>(reinterpret_cast<Slot *>(this->slots_buffer.ptr), this->capacity);
	auto keyvalues = std::span<KeyValue>(reinterpret_cast<KeyValue *>(this->keyvalues_buffer.ptr), this->capacity);

	// A temporary slot is created to trunc the hash to the same size as other slots
	Slot slot_to_find;
	slot_to_find.bits.hash = hash_value(key);

	u32 i_slot = power_of_2_modulo(slot_to_find.bits.hash, this->capacity);

	// Start probing to find a slot with a matching hash
	for (u32 i = 0; i < this->capacity; ++i) {
		i_slot = power_of_2_modulo((i_slot + i), this->capacity);

		if (slots[i_slot].bits.is_filled == 0) {
			return nullptr;
		}

		if (slots[i_slot].bits.is_filled == 1 && slots[i_slot].bits.hash == slot_to_find.bits.hash) {
			break;
		}
	}

	ASSERT(slots[i_slot].bits.is_filled);
	return &keyvalues[i_slot].value;
}

template <typename Key, typename Value>
u32 Map<Key, Value>::_insert_slot(Slot &slot_to_insert, KeyValue &value_to_insert)
{
	auto slots     = std::span<Slot>(reinterpret_cast<Slot *>(this->slots_buffer.ptr), this->capacity);
	auto keyvalues = std::span<KeyValue>(reinterpret_cast<KeyValue *>(this->keyvalues_buffer.ptr), this->capacity);

	u32 i_slot              = power_of_2_modulo(slot_to_insert.bits.hash, this->capacity);
	u32 i_original_key_slot = u32_invalid;

	// Start probing for an empty slot
	for (u32 i = 0; i < this->capacity; ++i) {
		i_slot = power_of_2_modulo((i_slot + i), this->capacity);

		// An empty slot if found
		if (slots[i_slot].bits.is_filled == 0) {
			if (i_original_key_slot == u32_invalid) {
				i_original_key_slot = i_slot;
			}
			break;
		}

		// Whenever the PSL of the key to insert becomes higher than the PSL of the probed key,
		// Swap them, the new key to insert becomes the probed key
		if (slot_to_insert.bits.psl > slots[i_slot].bits.psl) {
			i_original_key_slot = i_slot;
			std::swap(keyvalues[i_slot], value_to_insert);
			std::swap(slots[i_slot], slot_to_insert);
		}

		slot_to_insert.bits.psl += 1;
	}

	// Finally, insert the key at the empty slot
	slots[i_slot]     = slot_to_insert;
	keyvalues[i_slot] = std::move(value_to_insert);

	return i_original_key_slot;
}

template <typename Key, typename Value>
void Map<Key, Value>::_resize_and_rehash()
{
	auto new_capacity = this->capacity == 0 ? 2 : 2u * this->capacity;

	// Create the new buffers to hold slots and values
	DynamicBuffer new_slots_buffer     = {};
	DynamicBuffer new_keyvalues_buffer = {};
	DynamicBuffer::init(new_slots_buffer, new_capacity * sizeof(Slot));
	DynamicBuffer::init(new_keyvalues_buffer, new_capacity * sizeof(Map::KeyValue));

	// Update the map to point to new buffers, keep the old alloc to rehash slots
	auto          old_capacity          = this->capacity;
	DynamicBuffer old_slots_buffer      = this->slots_buffer;
	DynamicBuffer old_key_values_buffer = this->keyvalues_buffer;
	this->slots_buffer                  = new_slots_buffer;
	this->keyvalues_buffer              = new_keyvalues_buffer;
	this->capacity                      = new_capacity;

	// Rehash all filled values
	auto old_slots = std::span<Slot>(reinterpret_cast<Slot *>(old_slots_buffer.ptr), this->capacity);
	auto old_values =
		std::span<Map::KeyValue>(reinterpret_cast<Map::KeyValue *>(old_key_values_buffer.ptr), this->capacity);
	auto new_slots = std::span<Slot>(reinterpret_cast<Slot *>(new_slots_buffer.ptr), new_capacity);
	auto new_values =
		std::span<Map::KeyValue>(reinterpret_cast<Map::KeyValue *>(new_keyvalues_buffer.ptr), new_capacity);
	for (u32 i = 0; i < old_capacity; ++i) {
		if (old_slots[i].bits.is_filled) {
			this->_insert_slot(old_slots[i], old_values[i]);
		}
	}

	old_slots_buffer.destroy();
	old_key_values_buffer.destroy();
}

template <typename Key, typename Value>
Value *Map<Key, Value>::insert(Key key, Value &&value)
{
	auto max_load_size = (this->capacity * EXO_MAP_MAX_LOAD_FACTOR_NOM) / EXO_MAP_MAX_LOAD_FACTOR_DENOM;
	if (this->size + 1 > max_load_size) [[unlikely]] {
		this->_resize_and_rehash();
	}

	auto slots     = std::span<Slot>(reinterpret_cast<Slot *>(this->slots_buffer.ptr), this->capacity);
	auto keyvalues = std::span<KeyValue>(reinterpret_cast<KeyValue *>(this->keyvalues_buffer.ptr), this->capacity);

	auto value_to_insert = KeyValue{key, std::move(value)};
	Slot slot_to_insert;
	slot_to_insert.bits.is_filled = 1;
	slot_to_insert.bits.psl       = 0;
	slot_to_insert.bits.hash      = hash_value(key);

	u32 i_slot = this->_insert_slot(slot_to_insert, value_to_insert);

	ASSERT(i_slot < this->capacity);
	this->size += 1;
	return &keyvalues[i_slot].value;
}

template <typename Key, typename Value>
void Map<Key, Value>::remove(const Key &key)
{
	auto slots     = std::span<Slot>(reinterpret_cast<Slot *>(this->slots_buffer.ptr), this->capacity);
	auto keyvalues = std::span<KeyValue>(reinterpret_cast<KeyValue *>(this->keyvalues_buffer.ptr), this->capacity);

	// A temporary slot is created to trunc the hash to the same size as other slots
	Slot slot_to_find;
	slot_to_find.bits.hash = hash_value(key);

	u32 i_slot = power_of_2_modulo(slot_to_find.bits.hash, this->capacity);

	// Start probing to find a slot with a matching hash
	for (u32 i = 0; i < this->capacity; ++i) {
		i_slot = power_of_2_modulo((i_slot + i), this->capacity);

		// Early-return, the key was not found
		if (slots[i_slot].bits.is_filled == 0) {
			return;
		}

		if (slots[i_slot].bits.is_filled == 1 && slots[i_slot].bits.hash == slot_to_find.bits.hash) {
			break;
		}
	}

	// The key was found at slot i_slot, remove it and backward shift all values to fill the hole
	for (u32 i = 0; i < this->capacity; ++i) {
		auto current_slot = power_of_2_modulo((i_slot + i), this->capacity);
		auto next_slot    = power_of_2_modulo((i_slot + i + 1), this->capacity);

		if (slots[current_slot].bits.is_filled == 0 || slots[current_slot].bits.psl == 0) {
			break;
		}

		slots[current_slot]     = slots[next_slot];
		keyvalues[current_slot] = std::move(keyvalues[next_slot]);
	}

	this->size -= 1;
}
} // namespace exo
