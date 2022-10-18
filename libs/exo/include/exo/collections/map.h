#pragma once
#include <exo/collections/iterator_facade.h>
#include <exo/collections/span.h>
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

namespace details
{
union MapSlot
{
	struct
	{
		u32 is_filled : 1;
		u32 psl : 31; // probe sequence length, iterations needed to lookup element
		u32 hash;
	} bits;
	u64 raw;
};
} // namespace details

template <typename K, typename V>
struct MapIterator;

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
	inline ~Map()
	{
		this->keyvalues_buffer.destroy();
		this->slots_buffer.destroy();
	}

	Map() = default;

	Map(const Map &copy)            = delete;
	Map &operator=(const Map &copy) = delete;

	inline Map(Map &&moved) noexcept { *this = std::move(moved); }
	inline Map &operator=(Map &&moved) noexcept
	{
		this->capacity         = moved.capacity;
		this->size             = moved.size;
		this->keyvalues_buffer = std::move(moved.keyvalues_buffer);
		this->slots_buffer     = std::move(moved.slots_buffer);
		return *this;
	}

	// iterators
	MapIterator<Key, Value> begin() { return MapIterator<Key, Value>(this); }
	MapIterator<Key, Value> end() { return MapIterator<Key, Value>(this, this->capacity); }

	Value *at(const Key &key);
	Value *insert(Key key, Value &&value);
	Value *insert(Key key, const Value &value);
	void   remove(const Key &key);
};

namespace details
{
// "Fast" modulo, only works with power of 2 divisors
template <typename Unsigned>
inline constexpr Unsigned power_of_2_modulo(Unsigned a, Unsigned b)
{
	ASSERT(std::has_single_bit(b));
	return a & (b - 1);
}

inline u32 probe_by_hash(const Span<const MapSlot> slots, const u64 hash)
{
	// A temporary slot is created to trunc the hash to the same size as regular slots
	MapSlot slot_to_find;
	slot_to_find.bits.hash = hash;

	const u32 i_hash_slot = power_of_2_modulo(slot_to_find.bits.hash, u32(slots.size()));

	// Start probing to find a slot with a matching hash
	const u32 slots_length = u32(slots.size());
	for (u32 i = 0; i < slots_length; ++i) {
		const u32 i_slot = power_of_2_modulo((i_hash_slot + i), slots_length);

		if (slots[i_slot].bits.is_filled == 0) {
			return u32_invalid;
		}

		if (slots[i_slot].bits.is_filled == 1 && slots[i_slot].bits.hash == slot_to_find.bits.hash) {
			return i_slot;
		}
	}

	return u32_invalid;
}

template <typename T>
u32 insert_slot(Span<MapSlot> slots, Span<T> values, MapSlot &&slot, T &&value)
{
	// We need to keep track of the slot and value to insert to be able to swap them when needed
	MapSlot slot_to_insert  = std::move(slot);
	T       value_to_insert = std::move(value);

	const u32 slots_length = u32(slots.size());
	const u32 i_hash_slot  = power_of_2_modulo(slot_to_insert.bits.hash, slots_length);

	// Because we may "insert" multiple slots to reoder them, we need to keep track of the first "insert"
	u32 i_original_key_slot = u32_invalid;
	u32 i_slot              = 0;

	// Start probing for an empty slot
	for (u32 i = 0; i < slots_length; ++i) {
		i_slot             = power_of_2_modulo((i_hash_slot + i), slots_length);
		auto &current_slot = slots[i_slot];

		// An empty slot if found
		if (current_slot.bits.is_filled == 0) {
			if (i_original_key_slot == u32_invalid) {
				i_original_key_slot = i_slot;
			}
			break;
		}

		// Detect hash colisions
		ASSERT(current_slot.bits.hash != slot_to_insert.bits.hash);

		// Whenever the PSL of the key to insert becomes higher than the PSL of the probed key,
		// Swap them, the new key to insert becomes the probed key
		if (slot_to_insert.bits.psl > current_slot.bits.psl) {
			if (i_original_key_slot == u32_invalid) {
				i_original_key_slot = i_slot;
			}
			std::swap(values[i_slot], value_to_insert);
			std::swap(current_slot, slot_to_insert);
		}

		slot_to_insert.bits.psl += 1;
	}

	// Finally, insert the key at the empty slot
	slots[i_slot]  = slot_to_insert;
	values[i_slot] = std::move(value_to_insert);

	return i_original_key_slot;
}

template <typename T>
void resize_and_rehash(DynamicBuffer &slots_buffer, DynamicBuffer &keyvalues_buffer, u32 &capacity)
{
	auto new_capacity = capacity == 0 ? 2 : 2u * capacity;

	// Create the new buffers to hold slots and values
	DynamicBuffer new_slots_buffer     = {};
	DynamicBuffer new_keyvalues_buffer = {};
	DynamicBuffer::init(new_slots_buffer, new_capacity * sizeof(MapSlot));
	DynamicBuffer::init(new_keyvalues_buffer, new_capacity * sizeof(T));

	// Update the map to point to new buffers, keep the old alloc to rehash slots
	auto          old_capacity         = capacity;
	DynamicBuffer old_slots_buffer     = std::move(slots_buffer);
	DynamicBuffer old_keyvalues_buffer = std::move(keyvalues_buffer);

	// Rehash all filled values
	const auto old_slots  = exo::reinterpret_span<MapSlot>(old_slots_buffer.content());
	const auto old_values = exo::reinterpret_span<T>(old_keyvalues_buffer.content());
	const auto new_slots  = exo::reinterpret_span<MapSlot>(new_slots_buffer.content());
	const auto new_values = exo::reinterpret_span<T>(new_keyvalues_buffer.content());

	for (u32 i = 0; i < old_capacity; ++i) {
		if (old_slots[i].bits.is_filled) {
			old_slots[i].bits.psl = 0;
			details::insert_slot<T>(new_slots, new_values, std::move(old_slots[i]), std::move(old_values[i]));
		}
	}

	slots_buffer     = std::move(new_slots_buffer);
	keyvalues_buffer = std::move(new_keyvalues_buffer);
	capacity         = new_capacity;

	old_slots_buffer.destroy();
	old_keyvalues_buffer.destroy();
}
} // namespace details

template <typename Key, typename Value>
Map<Key, Value> Map<Key, Value>::with_capacity(u32 new_capacity)
{
	ASSERT(std::has_single_bit(new_capacity));

	Map map      = {};
	map.capacity = new_capacity;
	DynamicBuffer::init(map.keyvalues_buffer, new_capacity * sizeof(Map::KeyValue));
	DynamicBuffer::init(map.slots_buffer, new_capacity * sizeof(details::MapSlot));
	return map;
}

template <typename Key, typename Value>
Value *Map<Key, Value>::at(const Key &key)
{
	if (this->size == 0) {
		return nullptr;
	}

	const auto slots = exo::reinterpret_span<details::MapSlot>(this->slots_buffer.content());
	const auto hash  = hash_value(key);

	u32 i_slot = details::probe_by_hash(slots, hash);

	// key not found
	if (i_slot == u32_invalid) {
		return nullptr;
	}

	ASSERT(slots[i_slot].bits.is_filled);
	const auto keyvalues = exo::reinterpret_span<KeyValue>(this->keyvalues_buffer.content());
	return &keyvalues[i_slot].value;
}

template <typename Key, typename Value>
Value *Map<Key, Value>::insert(Key key, Value &&value)
{
	auto max_load_size = (this->capacity * EXO_MAP_MAX_LOAD_FACTOR_NOM) / EXO_MAP_MAX_LOAD_FACTOR_DENOM;
	if (this->size + 1 > max_load_size) [[unlikely]] {
		details::resize_and_rehash<KeyValue>(this->slots_buffer, this->keyvalues_buffer, this->capacity);
	}

	const auto slots     = exo::reinterpret_span<details::MapSlot>(this->slots_buffer.content());
	const auto keyvalues = exo::reinterpret_span<KeyValue>(this->keyvalues_buffer.content());

	details::MapSlot slot_to_insert;
	slot_to_insert.bits.is_filled = 1;
	slot_to_insert.bits.psl       = 0;
	slot_to_insert.bits.hash      = hash_value(key);
	u32 i_slot = details::insert_slot(slots, keyvalues, std::move(slot_to_insert), KeyValue{key, std::move(value)});

	ASSERT(i_slot < this->capacity);
	this->size += 1;
	return &keyvalues[i_slot].value;
}

template <typename Key, typename Value>
Value *Map<Key, Value>::insert(Key key, const Value &value)
{
	auto max_load_size = (this->capacity * EXO_MAP_MAX_LOAD_FACTOR_NOM) / EXO_MAP_MAX_LOAD_FACTOR_DENOM;
	if (this->size == 0 || this->size + 1 > max_load_size) [[unlikely]] {
		details::resize_and_rehash<KeyValue>(this->slots_buffer, this->keyvalues_buffer, this->capacity);
	}

	const auto slots     = exo::reinterpret_span<details::MapSlot>(this->slots_buffer.content());
	const auto keyvalues = exo::reinterpret_span<KeyValue>(this->keyvalues_buffer.content());

	details::MapSlot slot_to_insert;
	slot_to_insert.bits.is_filled = 1;
	slot_to_insert.bits.psl       = 0;
	slot_to_insert.bits.hash      = hash_value(key);
	u32 i_slot = details::insert_slot(slots, keyvalues, std::move(slot_to_insert), KeyValue{key, value});

	ASSERT(i_slot < this->capacity);
	this->size += 1;
	return &keyvalues[i_slot].value;
}

template <typename Key, typename Value>
void Map<Key, Value>::remove(const Key &key)
{
	const auto slots = exo::reinterpret_span<details::MapSlot>(this->slots_buffer.content());
	const auto hash  = hash_value(key);

	const u32 i_slot = details::probe_by_hash(slots, hash);

	// Not found
	if (i_slot == u32_invalid) {
		ASSERT(false);
		return;
	}

	const auto keyvalues = exo::reinterpret_span<KeyValue>(this->keyvalues_buffer.content());

	// The key was found at slot i_slot, remove it and backward shift all values to fill the hole
	for (u32 i = 0; i < this->capacity; ++i) {
		const auto current_slot = details::power_of_2_modulo((i_slot + i), this->capacity);
		const auto next_slot    = details::power_of_2_modulo((i_slot + i + 1), this->capacity);

		if (slots[next_slot].bits.is_filled == 0 || slots[next_slot].bits.psl == 0) {
			// All elements are shifted towards 0, so whenever we break, the current_slot was already copied to the
			// previous_slot
			slots[current_slot] = {};
			keyvalues[current_slot].~KeyValue();
			break;
		}

		slots[current_slot] = slots[next_slot];
		ASSERT(slots[current_slot].bits.psl != 0);
		slots[current_slot].bits.psl -= 1;
		keyvalues[current_slot] = std::move(keyvalues[next_slot]);
	}

	this->size -= 1;
}

// -- Iterators
template <typename K, typename V>
struct MapIterator : IteratorFacade<MapIterator<K, V>>
{
	using Map      = typename Map<K, V>;
	using KeyValue = typename Map::KeyValue;

	MapIterator() = default;
	MapIterator(Map *_Map, u32 _index = 0) : map{_Map}, current_index{_index}
	{
		const auto slots = exo::reinterpret_span<details::MapSlot>(this->map->slots_buffer.content());
		if (this->current_index < this->map->capacity && slots[this->current_index].bits.is_filled == 0) {
			this->increment();
		}
	}

	KeyValue &dereference() const
	{
		const auto keyvalues = exo::reinterpret_span<KeyValue>(this->map->keyvalues_buffer.content());
		return keyvalues[this->current_index];
	}

	void increment()
	{
		const auto slots = exo::reinterpret_span<details::MapSlot>(this->map->slots_buffer.content());

		for (current_index = current_index + 1; current_index < this->map->capacity; current_index += 1) {
			if (slots[current_index].bits.is_filled == 1) {
				break;
			}
		}
	}

	bool equal_to(const MapIterator &other) const
	{
		return this->map == other.map && this->current_index == other.current_index;
	}

	Map *map           = nullptr;
	u32  current_index = u32_invalid;
};

} // namespace exo
