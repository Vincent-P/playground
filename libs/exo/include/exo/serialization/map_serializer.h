#pragma once

#include "exo/collections/map.h"
#include "exo/serialization/serializer.h"

namespace exo
{
template <typename K, typename V>
void serialize(Serializer &serializer, Map<K, V> &map)
{
	using KeyValue = Map<K, V>::KeyValue;

	if (serializer.is_writing) {
		serialize(serializer, map.capacity);
		serialize(serializer, map.size);

		const auto slots     = exo::reinterpret_span<details::MapSlot>(map.slots_buffer.content());
		const auto keyvalues = exo::reinterpret_span<KeyValue>(map.keyvalues_buffer.content());

		for (usize i = 0; i < map.capacity; ++i) {
			if (slots[i].bits.is_filled == 1) {
				serialize(serializer, keyvalues[i].key);
				serialize(serializer, keyvalues[i].value);
			}
		}
	} else {
		u32 capacity = 0;
		u32 size     = 0;

		serialize(serializer, capacity);
		serialize(serializer, size);

		// Read values from the buffer
		DynamicBuffer tmp_values = {};
		DynamicBuffer::init(tmp_values, sizeof(KeyValue) * size);
		const auto tmp_keyvalues = exo::reinterpret_span<KeyValue>(tmp_values.content());
		ASSERT(tmp_keyvalues.size() == size);
		for (auto &keyvalue : tmp_keyvalues) {
			serialize(serializer, keyvalue.key);
			serialize(serializer, keyvalue.value);
		}

		// Insert all of them into the map
		map            = Map<K, V>::with_capacity(capacity);
		auto slots     = exo::reinterpret_span<details::MapSlot>(map.slots_buffer.content());
		auto keyvalues = exo::reinterpret_span<KeyValue>(map.keyvalues_buffer.content());
		for (auto &keyvalue : tmp_keyvalues) {
			details::MapSlot slot_to_insert;
			slot_to_insert.bits.is_filled = 1;
			slot_to_insert.bits.psl       = 0;
			slot_to_insert.bits.hash      = hash_value(keyvalue.key);
			exo::details::insert_slot<KeyValue>(slots, keyvalues, std::move(slot_to_insert), std::move(keyvalue));
		}
		map.size = size;

		tmp_values.destroy();
	}
}
} // namespace exo
