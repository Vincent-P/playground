#pragma once

#include "exo/collections/pool.h"
#include "exo/profile.h"
#include "exo/serializer.h"

namespace exo
{
template <typename T> void serialize(Serializer &serializer, Pool<T> &data)
{
	serialize(serializer, data.freelist_head);
	serialize(serializer, data.size);
	serialize(serializer, data.capacity);

	usize buffer_size = data.capacity * (Pool<T>::ELEMENT_SIZE() + sizeof(ElementMetadata));
	if (!serializer.is_writing && buffer_size > 0) {
		DynamicBuffer::init(data.buffer, buffer_size);
	}

	u32 i_element = 0;
	for (; i_element < data.capacity; i_element += 1) {
		auto *metadata = metadata_ptr(data, i_element);

		serialize(serializer, metadata->raw);
		if (metadata->bits.is_occupied) {
			auto *element = element_ptr(data, i_element);

			// If we are loading elements, we need to default-construct them to avoid garbage values in copy/move
			// constructors
			if (!serializer.is_writing) {
				new (element) T{};
			}

			serialize(serializer, *element);
		} else {
			auto *freelist = freelist_ptr(data, i_element);
			serialize(serializer, *freelist);
		}
	}
}
} // namespace exo
